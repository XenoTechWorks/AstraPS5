// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
namespace Astra.Shell.Models;

public enum GameSource { Flatpak, Steam }

public sealed record GameEntry(string Id, string Name, GameSource Source)
{
    public bool CanLaunch => Source == GameSource.Flatpak;
    public string SourceLabel => Source == GameSource.Flatpak ? "FLATPAK" : "STEAM";
    public string Availability => CanLaunch ? "Ready to launch on Linux PC" : "Discovery only — launching not supported";
}
