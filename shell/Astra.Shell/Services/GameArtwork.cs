using System.Text.RegularExpressions;
using Astra.Shell.Models;
using Avalonia.Media.Imaging;

namespace Astra.Shell.Services;

/// <summary>
/// Cover-first local artwork discovery. Never substitutes app icons for game covers.
/// Uses Steam's cached portrait/hero assets and optional user-supplied artwork.
/// </summary>
public static class GameArtwork
{
    private static readonly string Home = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
    private static readonly string Custom = Path.Combine(Home, ".local", "share", "astra", "artwork");
    // Keep successfully decoded covers in memory for the shell session. This
    // also prevents repeatedly resetting the background for the same game.
    private static readonly Dictionary<string, Bitmap> ImageCache = new(StringComparer.Ordinal);
    private static readonly Dictionary<string, string[]> SteamNameCache = new(StringComparer.OrdinalIgnoreCase);

    private static readonly string[] SteamRoots =
    [
        Path.Combine(Home, ".steam", "steam", "appcache", "librarycache"),
        Path.Combine(Home, ".local", "share", "Steam", "appcache", "librarycache"),
        Path.Combine(Home, ".var", "app", "com.valvesoftware.Steam", ".local", "share", "Steam", "appcache", "librarycache")
    ];

    public static Bitmap? FindCover(GameEntry game) => Load(Candidates(game, false));
    public static Bitmap? FindBackground(GameEntry game) => Load(Candidates(game, true));

    private static Bitmap? Load(IEnumerable<string> paths)
    {
        foreach (var path in paths)
        {
            try
            {
                lock (ImageCache)
                {
                    if (ImageCache.TryGetValue(path, out var cached)) return cached;
                    if (!File.Exists(path)) continue;
                    var decoded = new Bitmap(path);
                    ImageCache[path] = decoded;
                    return decoded;
                }
            }
            catch (Exception) { /* skip corrupt/unsupported local image */ }
        }
        return null;
    }

    private static IEnumerable<string> Candidates(GameEntry game, bool background)
    {
        // User overrides: ~/.local/share/astra/artwork/<app-id>/cover.jpg or background.jpg
        if (SafeName(game.Id))
        {
            foreach (var ext in new[] { ".jpg", ".png", ".jpeg", ".webp" })
                yield return Path.Combine(Custom, game.Id, (background ? "background" : "cover") + ext);
        }

        // Flatpak games sometimes also exist in Steam. Use their Steam library artwork
        // if a matching installed Steam appmanifest is found; never use a Flatpak icon.
        var ids = game.Source == GameSource.Steam && game.Id.All(char.IsDigit)
            ? new[] { game.Id }
            : SteamIdsByName(game.Name);
        foreach (var id in ids)
        foreach (var root in SteamRoots)
        {
            var names = background
                ? new[] { $"{id}_library_hero.jpg", $"{id}_library_hero.png", $"{id}/library_hero.jpg", $"{id}/library_hero.png" }
                : new[] { $"{id}_library_600x900.jpg", $"{id}_library_600x900.png", $"{id}/library_600x900.jpg", $"{id}/library_600x900.png" };
            foreach (var name in names) yield return Path.Combine(root, name);
        }
    }

    private static bool SafeName(string name) => name.Length is > 0 and < 180 &&
        name.All(c => char.IsLetterOrDigit(c) || c is '.' or '-' or '_');

    private static string[] SteamIdsByName(string name)
    {
        if (string.IsNullOrWhiteSpace(name)) return [];
        lock (SteamNameCache)
        {
            if (SteamNameCache.TryGetValue(name, out var known)) return known;
        }
        var matches = FindSteamIdsByName(name).ToArray();
        lock (SteamNameCache) SteamNameCache[name] = matches;
        return matches;
    }

    private static IEnumerable<string> FindSteamIdsByName(string name)
    {
        var steam = new[] { Path.Combine(Home, ".steam", "steam"),
            Path.Combine(Home, ".local", "share", "Steam") };
        var seen = new HashSet<string>();
        foreach (var root in steam)
        {
            var dirs = new[] { Path.Combine(root, "steamapps") };
            foreach (var dir in dirs)
            {
                if (!Directory.Exists(dir)) continue;
                IEnumerable<string> files;
                try { files = Directory.EnumerateFiles(dir, "appmanifest_*.acf").Take(500).ToArray(); }
                catch (IOException) { continue; }
                catch (UnauthorizedAccessException) { continue; }
                foreach (var file in files)
                {
                    string content;
                    try { content = File.ReadAllText(file); }
                    catch (IOException) { continue; }
                    catch (UnauthorizedAccessException) { continue; }
                    var match = Regex.Match(content, "\\\"name\\\"\\s+\\\"([^\\\"]+)\\\"", RegexOptions.IgnoreCase);
                    if (!match.Success || !string.Equals(match.Groups[1].Value, name, StringComparison.OrdinalIgnoreCase)) continue;
                    var id = Path.GetFileNameWithoutExtension(file).Replace("appmanifest_", "");
                    if (id.All(char.IsDigit) && seen.Add(id)) yield return id;
                }
            }
        }
    }
}
