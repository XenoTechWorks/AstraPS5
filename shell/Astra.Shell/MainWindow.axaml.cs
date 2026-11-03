// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
using System.Globalization;
using Astra.Shell.Models;
using Astra.Shell.Services;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Animation;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Markup.Xaml;
using Avalonia.Threading;


namespace Astra.Shell;

public sealed partial class MainWindow : Window
{
    private enum Screen { Home, Library, Details, Settings, Diagnostics, News }
    private readonly List<GameEntry> _games = [];
    private readonly List<Button> _homeButtons = [];
    private readonly List<GameEntry> _homeGames = [];
    private AstraCliClient? _core;
    private GameEntry? _selected;
    private Screen _screen;
    private int _homeIndex;
    private int _filter; // 0 all, 1 Flatpak, 2 Steam
    private bool _launching;
    private bool _welcomeShown;
    private readonly ShellPreferences _preferences = ShellPreferences.Load();
    private bool _showSeconds;
    private double _lastLayoutWidth;
    private DateTime _lastClockClick = DateTime.MinValue;
    private readonly DispatcherTimer _clock = new() { Interval = TimeSpan.FromSeconds(1) };
    private readonly DispatcherTimer _toastTimer = new() { Interval = TimeSpan.FromSeconds(4) };

    public MainWindow()
    {
        AvaloniaXamlLoader.Load(this);
        _showSeconds = _preferences.ShowSeconds;
        var waves = Find<WaveBackground>("Waves");
        waves.SetAnimationEnabled(_preferences.AnimateWallpaper);
        WireSettings();
        foreach (var name in new[] { "HomePage", "LibraryPage", "DetailsPage", "SettingsPage", "DiagnosticsPage", "NewsPage" })
            Find<Grid>(name).Transitions = new Transitions
            {
                new DoubleTransition { Property = OpacityProperty, Duration = TimeSpan.FromMilliseconds(260) }
            };
        UpdateClock();
        _clock.Tick += (_, _) => UpdateClock();
        _clock.Start();
        _toastTimer.Tick += (_, _) =>
        {
            _toastTimer.Stop();
            Find<Border>("NotificationToast").IsVisible = false;
        };
        Closed += (_, _) => { _clock.Stop(); _toastTimer.Stop(); };
        SizeChanged += (_, _) =>
        {
            if (Math.Abs(Bounds.Width - _lastLayoutWidth) < 80) return;
            _lastLayoutWidth = Bounds.Width;
            RenderHome();
            RenderLibrary();
        };
        Opened += async (_, _) => { await RefreshAsync(); ShowWelcomeOnce(); };
    }

    private void UpdateClock()
    {
        var now = DateTime.Now;
        Find<TextBlock>("TimeLabel").Text = now.ToString(_showSeconds ? "h:mm:ss tt" : "h:mm tt", CultureInfo.CurrentCulture);
        Find<TextBlock>("DateLabel").Text = now.ToString("ddd, MMM d", CultureInfo.CurrentCulture);
    }

    private void ClockClicked(object? sender, RoutedEventArgs e)
    {
        var now = DateTime.UtcNow;
        if ((now - _lastClockClick).TotalMilliseconds < 500)
        {
            _showSeconds = !_showSeconds;
            _preferences.ShowSeconds = _showSeconds;
            Find<CheckBox>("SecondsCheck").IsChecked = _showSeconds;
            _preferences.Save();
            UpdateClock();
            _lastClockClick = DateTime.MinValue;
        }
        else _lastClockClick = now;
    }

    private void WireSettings()
    {
        var animated = Find<CheckBox>("AnimationCheck");
        var backgrounds = Find<CheckBox>("GameBackgroundCheck");
        var seconds = Find<CheckBox>("SecondsCheck");
        var tiles = Find<ComboBox>("TileSizeChoice");
        animated.IsChecked = _preferences.AnimateWallpaper;
        backgrounds.IsChecked = _preferences.GameBackgrounds;
        seconds.IsChecked = _preferences.ShowSeconds;
        tiles.SelectedIndex = _preferences.TileSize;

        animated.IsCheckedChanged += SettingsToggleChanged;
        backgrounds.IsCheckedChanged += SettingsToggleChanged;
        seconds.IsCheckedChanged += SettingsToggleChanged;
        tiles.SelectionChanged += (_, _) =>
        {
            if (tiles.SelectedIndex < 0) return;
            _preferences.TileSize = tiles.SelectedIndex;
            _preferences.Save();
            RenderHome();
            RenderLibrary();
        };
    }

    private void SettingsToggleChanged(object? sender, RoutedEventArgs e)
    {
        _preferences.AnimateWallpaper = Find<CheckBox>("AnimationCheck").IsChecked == true;
        _preferences.GameBackgrounds = Find<CheckBox>("GameBackgroundCheck").IsChecked == true;
        _preferences.ShowSeconds = Find<CheckBox>("SecondsCheck").IsChecked == true;
        _showSeconds = _preferences.ShowSeconds;
        Find<WaveBackground>("Waves").SetAnimationEnabled(_preferences.AnimateWallpaper);
        UpdateBackground();
        UpdateClock();
        _preferences.Save();
    }

    // Poster-sized tiles, scaled to the window rather than stretched into giant squares.
    private (double Width, double Height) HomeTileSize()
    {
        var innerWidth = Math.Max(700, Bounds.Width - 85);
        var visible = Math.Clamp((int)Math.Floor(innerWidth / 188), 4, 9);
        var width = Math.Clamp((innerWidth - (visible - 1) * 22) / visible, 148, 205);
        width = Math.Clamp(width + (_preferences.TileSize - 1) * 18, 138, 230);
        return (width, width * 1.34);
    }

    private (double Width, double Height) LibraryTileSize()
    {
        var width = Math.Clamp(Bounds.Width / 8.4 + (_preferences.TileSize - 1) * 18, 150, 230);
        return (width, width * 1.34);
    }

    // Keep launchers and compatibility tools discoverable in Library, not the home games rail.
    private static bool IsUtility(GameEntry game) =>
        string.Equals(game.Name, "Steam", StringComparison.OrdinalIgnoreCase) ||
        game.Name.StartsWith("ProtonUp", StringComparison.OrdinalIgnoreCase) ||
        string.Equals(game.Name, "Vinegar", StringComparison.OrdinalIgnoreCase);

    private static string Initials(string name)
    {
        var words = name.Split(' ', StringSplitOptions.RemoveEmptyEntries);
        return words.Length switch
        {
            0 => "A",
            1 => words[0].Length >= 2 ? words[0][..2].ToUpperInvariant() : words[0].ToUpperInvariant(),
            _ => string.Concat(words.Take(2).Select(word => char.ToUpperInvariant(word[0])))
        };
    }

    private T Find<T>(string name) where T : Control => this.FindControl<T>(name)
        ?? throw new InvalidOperationException("Missing control: " + name);

    private void Show(Screen screen)
    {
        _screen = screen;
        var pages = new[]
        {
            (Screen.Home, Find<Grid>("HomePage")),
            (Screen.Library, Find<Grid>("LibraryPage")),
            (Screen.Details, Find<Grid>("DetailsPage")),
            (Screen.Settings, Find<Grid>("SettingsPage")),
            (Screen.Diagnostics, Find<Grid>("DiagnosticsPage")),
            (Screen.News, Find<Grid>("NewsPage"))
        };
        foreach (var (page, control) in pages)
        {
            var visible = screen == page;
            control.IsVisible = visible;
            if (visible)
            {
                control.Opacity = 0;
                Dispatcher.UIThread.Post(() => control.Opacity = 1, DispatcherPriority.Loaded);
            }
        }
        UpdateBackground();
    }

    private void SetStatus(string text)
    {
        // Transient feedback only. Never restore the old permanent status footer.
        if (text.StartsWith("Scanning", StringComparison.OrdinalIgnoreCase) ||
            text.Contains("discovered entries", StringComparison.OrdinalIgnoreCase)) return;
        Find<TextBlock>("NotificationText").Text = text;
        Find<Border>("NotificationToast").IsVisible = true;
        _toastTimer.Stop();
        _toastTimer.Start();
    }

    private void UpdateCover(string imageName, string fallbackName, GameEntry? game)
    {
        var cover = game is null ? null : GameArtwork.FindCover(game);
        var image = Find<Image>(imageName);
        image.Source = cover;
        image.IsVisible = cover is not null;
        var fallback = Find<TextBlock>(fallbackName);
        fallback.Text = game is null ? "ASTRA" : Initials(game.Name);
        fallback.IsVisible = cover is null;
    }

    private void ShowWelcomeOnce()
    {
        if (_welcomeShown) return;
        _welcomeShown = true;
        var marker = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "astra", "shell-welcome-v05");
        if (File.Exists(marker)) return;
        Find<Border>("WelcomeOverlay").IsVisible = true;
    }

    private void DismissWelcomeClicked(object? sender, RoutedEventArgs e)
    {
        Find<Border>("WelcomeOverlay").IsVisible = false;
        try
        {
            var directory = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "astra");
            Directory.CreateDirectory(directory);
            File.WriteAllText(Path.Combine(directory, "shell-welcome-v05"), "shown");
        }
        catch (IOException) { /* if settings are read-only, show welcome next time */ }
        catch (UnauthorizedAccessException) { }
    }

    private async Task RefreshAsync()
    {
        SetStatus("Scanning installed Flatpak and Steam games...");
        try
        {
            _core ??= new AstraCliClient();
            var entries = await _core.DiscoverAsync();
            _games.Clear();
            _games.AddRange(entries);
            _selected = _selected is null ? null : _games.FirstOrDefault(x => x.Id == _selected.Id && x.Source == _selected.Source);
            _selected ??= _games.FirstOrDefault();
            _homeIndex = _selected is null ? 0 : Math.Max(0, _games.IndexOf(_selected));
            RenderHome();
            RenderLibrary();
            SetStatus($"{_games.Count} discovered entries • Flatpak can launch • Steam discovery only • Linux PC");
        }
        catch (Exception error)
        {
            // Never substitute fabricated games for a failed discovery request.
            _games.Clear();
            _selected = null;
            _homeIndex = 0;
            RenderHome();
            RenderLibrary();
            SetStatus("Discovery unavailable: " + error.Message);
        }
    }

    private void RenderHome()
    {
        var row = Find<StackPanel>("HomeGameRow");
        row.Children.Clear();
        _homeButtons.Clear();
        _homeGames.Clear();
        _homeGames.AddRange(_games.Where(game => !IsUtility(game)).Take(20));
        if (_homeGames.Count == 0) _homeGames.AddRange(_games.Take(20));
        if (_homeGames.Count > 0)
        {
            var selectedIndex = _selected is null ? -1 : _homeGames.FindIndex(game => game.Id == _selected.Id && game.Source == _selected.Source);
            if (selectedIndex < 0) { _selected = _homeGames[0]; selectedIndex = 0; }
            _homeIndex = selectedIndex;
        }
        var (width, height) = HomeTileSize();
        foreach (var game in _homeGames)
        {
            var tile = MakeTile(game, width, height, true);
            tile.Click += (_, _) => { _selected = game; _homeIndex = _homeGames.IndexOf(game); UpdateHero(); OpenDetails(game); };
            row.Children.Add(tile);
            _homeButtons.Add(tile);
        }
        if (_games.Count == 0)
            row.Children.Add(new TextBlock { Text = "No games found yet. Refresh the library after installing a game.", Foreground = Brush.Parse("#B3CCE2"), TextWrapping = TextWrapping.Wrap, FontSize = 17, VerticalAlignment = VerticalAlignment.Center });
        Find<TextBlock>("GameCountLabel").Text = _homeGames.Count == 1 ? "1 title" : $"{_homeGames.Count} titles";
        UpdateHero();
    }

    private Button MakeTile(GameEntry game, double width, double height, bool onHome)
    {
        var tile = new Button
        {
            Width = width, Height = height,
            HorizontalContentAlignment = HorizontalAlignment.Stretch,
            VerticalContentAlignment = VerticalAlignment.Stretch,
            Margin = new Thickness(0, 0, 5, 12),
            Tag = new TileDimensions(width, height)
        };
        tile.Classes.Add("game-tile");
        tile.Transitions = new Transitions
        {
            new DoubleTransition { Property = WidthProperty, Duration = TimeSpan.FromMilliseconds(200) },
            new DoubleTransition { Property = HeightProperty, Duration = TimeSpan.FromMilliseconds(200) },
            new DoubleTransition { Property = OpacityProperty, Duration = TimeSpan.FromMilliseconds(200) }
        };

        // Real poster art wins. Missing covers use a distinctive typography card,
        // never a blown-up desktop application icon or a generic diamond.
        var contents = new Grid { ClipToBounds = true };
        var cover = GameArtwork.FindCover(game);
        if (cover is not null)
            contents.Children.Add(new Image { Source = cover, Stretch = Stretch.UniformToFill });
        else
        {
            var palettes = new (string Top, string Bottom, string Ink)[]
            {
                ("#245577", "#0A1E3A", "#A5E2FF"),
                ("#53456F", "#161A38", "#DDC1FF"),
                ("#23645F", "#092C35", "#A6F5DD"),
                ("#634F42", "#251E35", "#FFDAC3"),
                ("#344F80", "#0D1939", "#B3CFFF")
            };
            var hash = 0u;
            foreach (var c in game.Name) hash = unchecked(hash * 31u + (uint)c);
            var colors = palettes[(int)(hash % (uint)palettes.Length)];
            contents.Children.Add(new Border
            {
                Background = new LinearGradientBrush
                {
                    StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                    EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative),
                    GradientStops = new GradientStops
                    {
                        new GradientStop(Color.Parse(colors.Top), 0),
                        new GradientStop(Color.Parse(colors.Bottom), 1)
                    }
                }
            });
            contents.Children.Add(new TextBlock
            {
                Text = Initials(game.Name),
                FontSize = Math.Clamp(width * .35, 48, 73),
                FontWeight = FontWeight.Bold,
                Foreground = Brush.Parse(colors.Ink),
                Opacity = .85,
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center,
                Margin = new Thickness(0, 0, 0, 40)
            });
            contents.Children.Add(new Border
            {
                Background = Brush.Parse(colors.Ink),
                Width = 34,
                Height = 3,
                CornerRadius = new CornerRadius(2),
                HorizontalAlignment = HorizontalAlignment.Left,
                VerticalAlignment = VerticalAlignment.Top,
                Margin = new Thickness(16, 16, 0, 0),
                Opacity = .85
            });
        }

        var titleShade = new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(0, 1, RelativeUnit.Relative),
            GradientStops = new GradientStops
            {
                new GradientStop(Color.FromArgb(0, 2, 10, 27), 0),
                new GradientStop(Color.FromArgb(248, 2, 10, 27), 1)
            }
        };
        contents.Children.Add(new Border
        {
            VerticalAlignment = VerticalAlignment.Bottom,
            Background = titleShade,
            Padding = new Thickness(13, 28, 13, 14),
            Child = new TextBlock
            {
                Text = game.Name,
                FontSize = Math.Clamp(width * .087, 14, 18),
                FontWeight = FontWeight.SemiBold,
                Foreground = Brushes.White,
                TextWrapping = TextWrapping.Wrap,
                TextTrimming = TextTrimming.CharacterEllipsis,
                MaxLines = 2
            }
        });
        tile.Content = contents;

        if (onHome)
        {
            void Select()
            {
                _selected = game;
                _homeIndex = _homeGames.IndexOf(game);
                UpdateHero();
            }
            tile.PointerEntered += (_, _) => Select();
            tile.GotFocus += (_, _) => Select();
        }
        return tile;
    }

    private readonly record struct TileDimensions(double Width, double Height);

    private void HighlightTiles()
    {
        foreach (var tile in _homeButtons)
        {
            var active = _homeButtons.IndexOf(tile) == _homeIndex;
            tile.BorderBrush = Brush.Parse(active ? "#F1FAFF" : "#3C7DA9CF");
            tile.BorderThickness = new Thickness(active ? 3 : 1);
            tile.Opacity = active ? 1 : .87;
            var baseWidth = tile.Tag is TileDimensions dimensions ? dimensions.Width : tile.Width;
            var baseHeight = tile.Tag is TileDimensions dim ? dim.Height : tile.Height;
            tile.Width = baseWidth + (active ? 10 : 0);
            tile.Height = baseHeight + (active ? 13 : 0);
        }
    }

    private void UpdateBackground()
    {
        Find<WaveBackground>("Waves").SetArtwork(
            _preferences.GameBackgrounds && (_screen is Screen.Home or Screen.Details) && _selected is not null
                ? GameArtwork.FindBackground(_selected) : null);
    }

    private void UpdateHero()
    {
        var heroTitle = Find<TextBlock>("HeroTitle");
        heroTitle.Text = _selected?.Name ?? "Welcome to Astra Shell";
        heroTitle.FontSize = (_selected?.Name.Length ?? 0) > 30 ? 37 : 46;
        Find<TextBlock>("HeroSubtitle").Text = _selected is null ? "Your library will appear here."
            : _selected.CanLaunch ? "Ready to play" : "In your library · Launch support coming later";
        Find<Button>("HeroDetails").IsEnabled = _selected is not null && !_launching;
        Find<Button>("HeroDetails").Content = _selected?.CanLaunch == true ? "Play now" : "View game";
        UpdateCover("HeroCover", "HeroCoverFallback", _selected);
        UpdateBackground();
        if (_selected is null) return;
        HighlightTiles();
        // Keep keyboard selection and focus synced without making the library look populated by placeholders.
        for (var i = 0; i < _homeButtons.Count; i++)
            _homeButtons[i].Opacity = i == _homeIndex ? 1 : .87;
    }

    private void RenderLibrary()
    {
        string search = Find<TextBox>("SearchBox").Text?.Trim() ?? "";
        var grid = Find<WrapPanel>("LibraryGameGrid");
        var (tileWidth, tileHeight) = LibraryTileSize();
        grid.ItemWidth = tileWidth + 17;
        grid.ItemHeight = tileHeight + 22;
        grid.Children.Clear();
        foreach (var game in _games.Where(g => (_filter == 0 || (_filter == 1 && g.Source == GameSource.Flatpak) ||
                    (_filter == 2 && g.Source == GameSource.Steam)) && g.Name.Contains(search, StringComparison.OrdinalIgnoreCase)))
        {
            var tile = MakeTile(game, tileWidth, tileHeight, false);
            tile.Click += (_, _) => OpenDetails(game);
            grid.Children.Add(tile);
        }
        if (grid.Children.Count == 0)
            grid.Children.Add(new TextBlock { Text = "No games match this filter. Try a different source or refresh.",
                Foreground = Brush.Parse("#A8C0D8"), FontSize = 16, Margin = new Thickness(10, 20) });
    }

    private void OpenDetails(GameEntry game)
    {
        _selected = game;
        var detailsTitle = Find<TextBlock>("DetailsTitle");
        detailsTitle.Text = game.Name;
        detailsTitle.FontSize = game.Name.Length > 34 ? 36 : 48;
        Find<TextBlock>("DetailsSource").Text = game.SourceLabel;
        Find<TextBlock>("DetailsAvailability").Text = game.Availability;
        Find<TextBlock>("DetailsId").Text = "App ID: " + game.Id;
        Find<Button>("DetailsPlay").IsEnabled = game.CanLaunch && !_launching;
        UpdateCover("DetailsCover", "DetailsCoverFallback", game);
        Show(Screen.Details);
    }

    private async Task PlayAsync()
    {
        if (_selected is null || !_selected.CanLaunch || _launching) return;
        var game = _selected;
        _launching = true;
        UpdateHero();
        Find<Button>("DetailsPlay").IsEnabled = false;
        SetStatus("Launching " + game.Name + " with Astra Core...");
        try
        {
            _core ??= new AstraCliClient();
            await _core.LaunchFlatpakAsync(game);
            SetStatus(game.Name + " exited normally. Session logs are managed by Astra Core.");
        }
        catch (Exception error) { SetStatus("Launch failed: " + error.Message); }
        finally
        {
            _launching = false;
            UpdateHero();
            Find<Button>("DetailsPlay").IsEnabled = _selected?.CanLaunch == true;
        }
    }

    private async Task LoadDiagnosticsAsync()
    {
        Show(Screen.Diagnostics);
        var text = Find<TextBlock>("DiagnosticsText");
        text.Text = "Collecting diagnostics...";
        try
        {
            _core ??= new AstraCliClient();
            text.Text = await _core.DiagnosticsAsync();
        }
        catch (Exception error) { text.Text = "Astra Core diagnostics unavailable: " + error.Message; }
    }

    private void GameSettingsClicked(object? sender, RoutedEventArgs e)
    {
        if (_selected is null) return;
        var game = _selected;
        var steam = game.Source == GameSource.Steam;
        var window = new Window
        {
            Title = game.Name + " — Game settings", Width = 680, Height = 470,
            MinWidth = 520, MinHeight = 380,
            Background = Brush.Parse("#071B39"), Foreground = Brushes.White
        };
        var content = new StackPanel { Spacing = 18, Margin = new Thickness(36, 24) };
        content.Children.Add(new TextBlock { Text = "Game settings", FontSize = 32, FontWeight = FontWeight.Bold });
        content.Children.Add(new TextBlock { Text = game.Name, FontSize = 19, Foreground = Brush.Parse("#AECBEA"), TextWrapping = TextWrapping.Wrap });
        content.Children.Add(new TextBlock { Text = "COMPATIBILITY", FontSize = 13, FontWeight = FontWeight.Bold, Foreground = Brush.Parse("#7DC7FF") });
        var proton = new ComboBox { IsEnabled = false, Width = 340, HorizontalAlignment = HorizontalAlignment.Left,
            ItemsSource = new[] { "Default (not configurable yet)" }, SelectedIndex = 0 };
        content.Children.Add(new TextBlock { Text = "Proton version", FontSize = 17 });
        content.Children.Add(proton);
        content.Children.Add(new TextBlock { Text = steam
            ? "Steam titles are currently discovery-only. Astra Core cannot launch them or select a Proton build yet."
            : "Flatpak games run through Astra Core. They do not use Astra-managed Proton selection.",
            FontSize = 15, TextWrapping = TextWrapping.Wrap, Foreground = Brush.Parse("#ADC5E4") });
        content.Children.Add(new TextBlock { Text = "LAUNCH OPTIONS", FontSize = 13, FontWeight = FontWeight.Bold, Foreground = Brush.Parse("#7DC7FF") });
        content.Children.Add(new TextBox { Watermark = "Custom launch arguments — unavailable until backend support", IsEnabled = false });
        content.Children.Add(new TextBlock { Text = "These settings are intentionally read-only until the C++ launcher implements them. No pretend save button.", TextWrapping = TextWrapping.Wrap, Foreground = Brush.Parse("#ADC5E4") });
        var close = new Button { Content = "Close", Width = 120, HorizontalAlignment = HorizontalAlignment.Left };
        close.Click += (_, _) => window.Close();
        content.Children.Add(close);
        window.Content = new ScrollViewer { Content = content };
        window.ShowDialog(this);
    }

    private void SettingsClicked(object? sender, RoutedEventArgs e) => Show(Screen.Settings);
    private void HelpClicked(object? sender, RoutedEventArgs e) => Find<Border>("WelcomeOverlay").IsVisible = true;
    private void NewsClicked(object? sender, RoutedEventArgs e) => Show(Screen.News);
    private void ReturnHome()
    {
        // A utility can be opened from Library without becoming a home-rail game.
        if (_homeGames.Count > 0 && (_selected is null || !_homeGames.Contains(_selected)))
        {
            _selected = _homeGames[0];
            _homeIndex = 0;
            UpdateHero();
        }
        Show(Screen.Home);
    }
    private void HomeClicked(object? sender, RoutedEventArgs e) => ReturnHome();
    private void LibraryClicked(object? sender, RoutedEventArgs e) { RenderLibrary(); Show(Screen.Library); }
    private void DetailsClicked(object? sender, RoutedEventArgs e) { if (_selected is not null) OpenDetails(_selected); }
    private async void HeroActionClicked(object? sender, RoutedEventArgs e)
    {
        if (_selected is null) return;
        if (_selected.CanLaunch) await PlayAsync();
        else OpenDetails(_selected);
    }
    private async void PlayClicked(object? sender, RoutedEventArgs e) => await PlayAsync();
    private async void RefreshClicked(object? sender, RoutedEventArgs e) => await RefreshAsync();
    private async void DiagnosticsClicked(object? sender, RoutedEventArgs e) => await LoadDiagnosticsAsync();
    private void ExitClicked(object? sender, RoutedEventArgs e) => Close();
    private void SearchChanged(object? sender, TextChangedEventArgs e) { if (_screen == Screen.Library) RenderLibrary(); }
    private void FilterClicked(object? sender, RoutedEventArgs e)
    {
        _filter = (_filter + 1) % 3;
        Find<Button>("FilterButton").Content = _filter switch { 1 => "Installed apps", 2 => "Steam games", _ => "All games" };
        RenderLibrary();
    }

    private void OnKeyDown(object? sender, KeyEventArgs e)
    {
        if (e.Key == Key.Escape || e.Key == Key.Back)
        {
            if (_screen != Screen.Home) ReturnHome();
            e.Handled = true;
        }
        else if (_screen == Screen.Home && _selected is not null && e.Key == Key.Enter)
        {
            OpenDetails(_selected);
            e.Handled = true;
        }
        else if (_screen == Screen.Home && _homeButtons.Count > 0 && (e.Key == Key.Left || e.Key == Key.Right))
        {
            _homeIndex = Math.Clamp(_homeIndex + (e.Key == Key.Right ? 1 : -1), 0, _homeButtons.Count - 1);
            _selected = _homeGames[_homeIndex];
            UpdateHero();
            _homeButtons[_homeIndex].Focus();
            e.Handled = true;
        }
    }
}
