// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
using Avalonia;

namespace Astra.Shell;

internal static class Program
{
    [STAThread]
    private static void Main(string[] args) => BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);

    private static AppBuilder BuildAvaloniaApp() => AppBuilder.Configure<App>()
        .UsePlatformDetect()
        .LogToTrace();
}
