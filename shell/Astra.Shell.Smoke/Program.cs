// Copyright 2026 Xeno Tech
// SPDX-License-Identifier: Apache-2.0
using Astra.Shell.Models;
using Astra.Shell.Services;

const string flatpak = """
{"platform":"linux-pc","ps5_tested":false,"games":[{"name":"SuperTuxKart","app_id":"net.supertuxkart.SuperTuxKart","runtime":"flatpak"}]}
""";
const string steam = """
{"platform":"linux-pc","ps5_tested":false,"games":[{"name":"Test Steam title","app_id":"480","install_path":"/tmp/steamapps/common/test"}]}
""";
var games = GameCatalogParser.Parse(flatpak, steam);
if (games.Count != 2 || games[0].Source != GameSource.Flatpak || !games[0].CanLaunch ||
    games[1].Source != GameSource.Steam || games[1].CanLaunch)
    throw new Exception("Game catalog parsing or source capability failed.");
var empty = GameCatalogParser.Parse("{\"games\":[]}", "{\"games\":[]}");
if (empty.Count != 0) throw new Exception("An empty install must not create fake games.");
var duplicate = GameCatalogParser.Parse("{\"games\":[{\"name\":\"Example\",\"app_id\":\"x.y\"},{\"name\":\"Example\",\"app_id\":\"x.y\"}]}", "{\"games\":[]}");
if (duplicate.Count != 1) throw new Exception("Duplicate entries were not removed.");
bool rejected = false;
try { GameCatalogParser.Parse("{\"bogus\":true}", steam); }
catch (FormatException) { rejected = true; }
if (!rejected) throw new Exception("Malformed discovery response must be rejected.");
Console.WriteLine("Astra Shell parser smoke tests passed.");
