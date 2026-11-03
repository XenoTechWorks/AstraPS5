// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
using System.Diagnostics;
using Astra.Shell.Models;

namespace Astra.Shell.Services;

/// <summary>Process boundary for the existing C++ CLI. No shell, no arbitrary Exec fields.</summary>
public sealed class AstraCliClient
{
    public string Executable { get; }

    public AstraCliClient(string? executable = null)
    {
        Executable = executable ?? FindExecutable();
    }

    private static string FindExecutable()
    {
        string? configured = Environment.GetEnvironmentVariable("ASTRA_BIN");
        if (!string.IsNullOrWhiteSpace(configured))
            return Path.GetFullPath(configured);

        // A convenient development default; production launchers must set ASTRA_BIN.
        foreach (var root in new[] { Directory.GetCurrentDirectory(), AppContext.BaseDirectory })
        {
            var dir = new DirectoryInfo(root);
            for (var depth = 0; dir is not null && depth < 7; depth++, dir = dir.Parent)
            {
                var candidate = Path.Combine(dir.FullName, "build", "astra");
                if (File.Exists(candidate)) return candidate;
            }
        }
        throw new FileNotFoundException("Astra Core binary not found. Build ./build/astra or set ASTRA_BIN to its absolute path.");
    }

    public async Task<IReadOnlyList<GameEntry>> DiscoverAsync(CancellationToken token = default)
    {
        // No simulated games: partial failures are surfaced, not hidden as an empty library.
        var flatpak = await RunAsync(["games", "list", "--json"], TimeSpan.FromSeconds(15), token);
        var steam = await RunAsync(["steam", "list", "--json"], TimeSpan.FromSeconds(15), token);
        return GameCatalogParser.Parse(flatpak, steam);
    }

    public Task<string> DiagnosticsAsync(CancellationToken token = default) =>
        RunAsync(["doctor"], TimeSpan.FromSeconds(15), token);

    public Task<string> LaunchFlatpakAsync(GameEntry game, CancellationToken token = default)
    {
        if (!game.CanLaunch) throw new InvalidOperationException("Steam launching is not implemented.");
        return RunAsync(["games", "run", game.Id], null, token);
    }

    private async Task<string> RunAsync(string[] arguments, TimeSpan? timeout, CancellationToken token)
    {
        if (!File.Exists(Executable)) throw new FileNotFoundException("Astra Core binary is missing: " + Executable);
        using var linked = CancellationTokenSource.CreateLinkedTokenSource(token);
        if (timeout is not null) linked.CancelAfter(timeout.Value);
        var start = new ProcessStartInfo
        {
            FileName = Executable,
            UseShellExecute = false,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            CreateNoWindow = true
        };
        foreach (var argument in arguments) start.ArgumentList.Add(argument);
        using var process = Process.Start(start) ?? throw new IOException("Failed to start Astra Core.");
        try
        {
            var stdout = process.StandardOutput.ReadToEndAsync(linked.Token);
            var stderr = process.StandardError.ReadToEndAsync(linked.Token);
            await process.WaitForExitAsync(linked.Token);
            var output = await stdout;
            var error = await stderr;
            if (process.ExitCode != 0)
                throw new InvalidOperationException($"astra {arguments[0]} failed (exit {process.ExitCode}): {error.Trim()}");
            return output;
        }
        catch (OperationCanceledException)
        {
            if (!process.HasExited) process.Kill(entireProcessTree: true);
            throw;
        }
    }
}
