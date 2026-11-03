using Avalonia;
using Avalonia.Controls;
using Avalonia.Media;
using Avalonia.Threading;

namespace Astra.Shell;

/// <summary>
/// Low-contrast animated atmospheric depth field: drifting aurora pools,
/// distant particles and a radial horizon. No banded waves or external assets.
/// </summary>
public sealed class WaveBackground : Control
{
    private readonly DispatcherTimer _timer;
    private double _phase;
    private bool _animationEnabled = true;
    private bool _attached;
    private IImage? _artwork;
    private IImage? _lastArtwork;
    private double _artworkBlend = 1;

    public WaveBackground()
    {
        IsHitTestVisible = false;
        _timer = new DispatcherTimer { Interval = TimeSpan.FromMilliseconds(40) };
        _timer.Tick += (_, _) =>
        {
            _phase += .014;
            if (_artworkBlend < 1)
            {
                _artworkBlend = Math.Min(1, _artworkBlend + .055);
                if (_artworkBlend >= 1) _lastArtwork = null;
            }
            InvalidateVisual();
        };
        AttachedToVisualTree += (_, _) =>
        {
            _attached = true;
            if (_animationEnabled) _timer.Start();
        };
        DetachedFromVisualTree += (_, _) =>
        {
            _attached = false;
            _timer.Stop();
        };
    }

    public void SetAnimationEnabled(bool enabled)
    {
        _animationEnabled = enabled;
        if (_attached && enabled) _timer.Start();
        else
        {
            _timer.Stop();
            _artworkBlend = 1;
            _lastArtwork = null;
        }
        InvalidateVisual();
    }

    public void SetArtwork(IImage? image)
    {
        if (ReferenceEquals(_artwork, image)) return;
        // Existing image references might be used by UI tiles; do not dispose them here.
        _lastArtwork = _artwork;
        _artwork = image;
        _artworkBlend = _animationEnabled ? 0 : 1;
        if (!_animationEnabled) _lastArtwork = null;
        InvalidateVisual();
    }

    private static void DrawCover(DrawingContext context, IImage artwork, double width, double height, double opacity)
    {
        if (opacity <= 0 || artwork.Size.Width <= 0 || artwork.Size.Height <= 0) return;
        var scale = Math.Max(width / artwork.Size.Width, height / artwork.Size.Height);
        var drawWidth = artwork.Size.Width * scale;
        var drawHeight = artwork.Size.Height * scale;
        using (context.PushOpacity(Math.Clamp(opacity, 0, 1)))
            context.DrawImage(artwork, new Rect((width - drawWidth) / 2, (height - drawHeight) / 2, drawWidth, drawHeight));
    }

    public override void Render(DrawingContext context)
    {
        base.Render(context);
        var w = Bounds.Width;
        var h = Bounds.Height;
        if (w <= 0 || h <= 0) return;
        context.FillRectangle(new LinearGradientBrush
        {
            StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
            EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative),
            GradientStops = new GradientStops
            {
                new GradientStop(Color.Parse("#030A1B"), 0),
                new GradientStop(Color.Parse("#08264C"), .48),
                new GradientStop(Color.Parse("#080E29"), 1)
            }
        }, new Rect(0, 0, w, h));

        // Soft parallax volumes give an impression of distance instead of rigid ribbons.
        for (var i = 0; i < 7; i++)
        {
            var depth = (i + 1.0) / 7;
            var drift = _phase * (.09 + depth * .12);
            var x = w * (.16 + depth * .68) + Math.Sin(drift + i * 2.4) * w * .10;
            var y = h * (.12 + depth * .72) + Math.Cos(drift * .65 + i * 1.7) * h * .08;
            var r = Math.Max(w, h) * (.18 + depth * .25);
            var c = i % 3 == 0 ? Color.Parse("#48B0D9") : i % 3 == 1 ? Color.Parse("#4D79DA") : Color.Parse("#5A53A7");
            var glow = new RadialGradientBrush
            {
                Center = new RelativePoint(.5, .5, RelativeUnit.Relative),
                GradientOrigin = new RelativePoint(.5, .5, RelativeUnit.Relative),
                Radius = .5,
                GradientStops = new GradientStops
                {
                    new GradientStop(Color.FromArgb((byte)(15 + 14 * depth), c.R, c.G, c.B), 0),
                    new GradientStop(Color.FromArgb((byte)(5 + 6 * depth), c.R, c.G, c.B), .54),
                    new GradientStop(Colors.Transparent, 1)
                }
            };
            context.DrawEllipse(glow, null, new Point(x, y), r, r * .8);
        }

        // Small slow-moving depth particles, deterministically placed, not random flicker.
        for (var i = 0; i < 44; i++)
        {
            var z = .25 + ((i * 37) % 100) / 130.0;
            var x = ((i * 179) % 997) / 997.0 * w + Math.Sin(_phase * z * .13 + i) * 9 * z;
            var y = ((i * 293) % 991) / 991.0 * h + Math.Cos(_phase * z * .16 + i) * 8 * z;
            var alpha = (byte)(12 + z * 26);
            context.DrawEllipse(new SolidColorBrush(Color.FromArgb(alpha, 173, 222, 255)), null,
                new Point(x, y), .5 + z * 1.1, .5 + z * 1.1);
        }

        // Game backgrounds crossfade instead of instantly swapping on hover.
        // Keep the atmospheric field underneath, then dim the game art for text contrast.
        if (_lastArtwork is not null) DrawCover(context, _lastArtwork, w, h, 1 - _artworkBlend);
        if (_artwork is not null) DrawCover(context, _artwork, w, h, _artworkBlend);
        if (_lastArtwork is not null || _artwork is not null)
        {
            context.FillRectangle(new LinearGradientBrush
            {
                StartPoint = new RelativePoint(0, 0, RelativeUnit.Relative),
                EndPoint = new RelativePoint(1, 1, RelativeUnit.Relative),
                GradientStops = new GradientStops
                {
                    new GradientStop(Color.FromArgb(215, 2, 11, 29), 0),
                    new GradientStop(Color.FromArgb(159, 3, 15, 36), .60),
                    new GradientStop(Color.FromArgb(202, 3, 9, 26), 1)
                }
            }, new Rect(0, 0, w, h));
        }
    }
}
