// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
using System.Text.Json;

namespace Astra.Shell;

/// <summary>UI-only preferences. Does not alter Astra Core, Steam or Proton settings.</summary>
public sealed class ShellPreferences
{
    public bool AnimateWallpaper { get; set; } = true;
    public bool GameBackgrounds { get; set; } = true;
    public bool ShowSeconds { get; set; }
    public int TileSize { get; set; } = 1; // 0 compact, 1 standard, 2 large

    private static string SettingsPath => Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
        "astra", "shell-settings.json");

    public static ShellPreferences Load()
    {
        try
        {
            if (!File.Exists(SettingsPath)) return new ShellPreferences();
            var loaded = JsonSerializer.Deserialize<ShellPreferences>(File.ReadAllText(SettingsPath));
            if (loaded is null) return new ShellPreferences();
            loaded.TileSize = Math.Clamp(loaded.TileSize, 0, 2);
            return loaded;
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException or JsonException)
        {
            return new ShellPreferences();
        }
    }

    public void Save()
    {
        try
        {
            Directory.CreateDirectory(Path.GetDirectoryName(SettingsPath)!);
            var temporary = SettingsPath + ".tmp";
            File.WriteAllText(temporary, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
            File.Move(temporary, SettingsPath, true);
        }
        catch (Exception error) when (error is IOException or UnauthorizedAccessException)
        {
            // A read-only home directory should not crash the shell.
        }
    }
}
