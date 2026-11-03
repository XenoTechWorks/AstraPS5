// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
using System.Text.Json;
using Astra.Shell.Models;

namespace Astra.Shell.Services;

/// <summary>Parses the existing, version 0.2.5 Astra CLI JSON without inventing entries.</summary>
public static class GameCatalogParser
{
    public static IReadOnlyList<GameEntry> Parse(string flatpakJson, string steamJson)
    {
        var results = new List<GameEntry>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        Add(flatpakJson, GameSource.Flatpak, results, seen);
        Add(steamJson, GameSource.Steam, results, seen);
        return results.OrderBy(g => g.Name, StringComparer.OrdinalIgnoreCase)
            .ThenBy(g => g.Source).ThenBy(g => g.Id, StringComparer.Ordinal).ToArray();
    }

    private static void Add(string json, GameSource source, List<GameEntry> output, HashSet<string> seen)
    {
        using var document = JsonDocument.Parse(json);
        if (!document.RootElement.TryGetProperty("games", out var games) ||
            games.ValueKind != JsonValueKind.Array)
            throw new FormatException($"{source} response does not contain a games array.");

        foreach (var item in games.EnumerateArray())
        {
            if (!item.TryGetProperty("app_id", out var appId) ||
                !item.TryGetProperty("name", out var gameName) ||
                appId.ValueKind != JsonValueKind.String || gameName.ValueKind != JsonValueKind.String)
                continue;
            var id = appId.GetString();
            var name = gameName.GetString();
            if (string.IsNullOrWhiteSpace(id) || string.IsNullOrWhiteSpace(name) ||
                id.Length > 190 || name.Length > 128 ||
                id.Any(char.IsControl) || name.Any(char.IsControl) ||
                !seen.Add(source + ":" + id))
                continue;
            output.Add(new GameEntry(id, name, source));
        }
    }
}
