# Builds the release archive: a FOMOD installer (Mod Organizer 2, Vortex) that installs the mod with
# its settings menu in one language. Each language folder holds the whole mod with the MCM texts in
# that language, under the same file names, and the installer copies one of them into Data:
#
#   core/<lang>/        the mod in that language (en, ja)
#   fomod/              the installer (ModuleConfig.xml, info.xml)
#
# It reads the built DLL, scripts, prompt bundle, MCM config and English texts from the mod
# project's assets/, the ESP from its build/, the Japanese texts from translations/ja/ (written by
# tools/make_strings.py) and the licence texts given, then writes dist/SN_SexualHistory_<version>.zip
# and reads it back as an installer would, checking every choice. Installing without the
# installer: copy the contents of core/<lang>/ into Data.
#
# usage: tools\build_fomod.ps1 [-Version 0.2.0]
param(
    [string]$Version,
    [string]$Assets = 'C:\Modding\skyrim-modding-tools-mcp\projects\SN_SexualHistory\assets',
    [string]$Esp = 'C:\Modding\skyrim-modding-tools-mcp\projects\SN_SexualHistory\build\SN_SexualHistory.esp',
    [string]$Sources = 'C:\Modding\skyrim-modding-tools-mcp\projects\SN_SexualHistory\src',
    [string]$Notices = 'C:\Modding\SN_SexualHistory_github\THIRD_PARTY_NOTICES.txt',
    [string]$Copying = 'C:\Modding\SN_SexualHistory_release\mod_0.1.0\COPYING.txt'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $Version) {
    $Version = (Select-String -Path "$root\xmake.lua" -Pattern 'PLUGIN_VERSION = "([^"]+)"').Matches[0].Groups[1].Value
}
$name = 'SN_SexualHistory'
$stage = "$root\dist\fomod"
$zip = "$root\dist\${name}_$Version.zip"
$gameLanguages = 'CHINESE', 'CZECH', 'ENGLISH', 'FRENCH', 'GERMAN', 'ITALIAN', 'JAPANESE', 'POLISH', 'RUSSIAN', 'SPANISH'
$languages = @(
    @{ code = 'en'; flag = 'en'; name = 'English'; desc = 'The settings menu in English. Works with any game language.'; recommended = $true },
    @{ code = 'ja'; flag = 'ja'; name = '日本語 (Japanese)'; desc = '設定メニューを日本語にします。ゲーム本体の言語は問いません。 The settings menu in Japanese.' }
)

foreach ($f in $Esp, $Notices, $Copying, "$Assets\SKSE\Plugins\$name.dll", "$root\translations\ja\Interface\Translations\${name}_JAPANESE.txt") {
    if (-not (Test-Path $f)) { throw "missing: $f" }
}
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory "$stage\fomod" | Out-Null

# ---- one complete mod per language ----
foreach ($l in $languages) {
    $dir = "$stage\core\$($l.code)"
    New-Item -ItemType Directory $dir | Out-Null
    Copy-Item "$Assets\*" $dir -Recurse
    Copy-Item $Esp $dir
    Copy-Item $Copying "$dir\COPYING.txt"
    Copy-Item $Notices "$dir\THIRD_PARTY_NOTICES.txt"
    New-Item -ItemType Directory "$dir\Source\Scripts" | Out-Null
    Copy-Item "$Sources\*.psc" "$dir\Source\Scripts"
    if ($l.code -ne 'en') {
        # the MCM texts and the relationship map's texts (strings.json) of that language
        Copy-Item "$root\translations\$($l.code)\*" $dir -Recurse -Force
    }
}

# ---- the installer ----
# Vortex re-encodes the XML as UTF-16 and only strips an encoding written in double quotes: a
# single-quoted one fails at line 1. No BOM either. The mod managers name the installed mod after
# info.xml's Name, so it stays "SN_SexualHistory" (the plugin's name; the mod is shown as SN Sexual
# Memories), the name the plain archives of 0.1.x got from their file name: updating replaces the
# old mod instead of adding a second one.
$utf8 = New-Object System.Text.UTF8Encoding($false)
$esc = { param($s) [System.Security.SecurityElement]::Escape($s) }
$info = @"
<?xml version="1.0" encoding="UTF-8"?>
<fomod>
  <Name>SN_SexualHistory</Name>
  <Author>nightlantern-mods</Author>
  <Version>$Version</Version>
  <Website>https://github.com/nightlantern-mods/SN_SexualMemories</Website>
  <Description>Adult content. A SkyrimNet add-on for OStim or SexLab: every scene becomes a memory for each character who took part, NPCs speak about it, and a relationship map shows what lies between people (with Magelight UI or Prisma UI). Choose the language of the settings menu.</Description>
  <Groups>
    <element>Gameplay</element>
  </Groups>
</fomod>
"@
$plugins = ($languages | ForEach-Object {
    $type = if ($_.recommended) { 'Recommended' } else { 'Optional' }
    @"
            <plugin name="$(& $esc $_.name)">
              <description>$(& $esc $_.desc)</description>
              <conditionFlags>
                <flag name="Language">$($_.flag)</flag>
              </conditionFlags>
              <typeDescriptor>
                <type name="$type"/>
              </typeDescriptor>
            </plugin>
"@
}) -join "`n"
$patterns = ($languages | ForEach-Object {
    @"
      <pattern>
        <dependencies operator="And">
          <flagDependency flag="Language" value="$($_.flag)"/>
        </dependencies>
        <files>
          <folder source="core\$($_.code)" destination="" priority="0"/>
        </files>
      </pattern>
"@
}) -join "`n"
$config = @"
<?xml version="1.0" encoding="UTF-8"?>
<config xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xsi:noNamespaceSchemaLocation="http://qconsulting.ca/fo3/ModConfig5.0.xsd">
  <moduleName>SN Sexual Memories</moduleName>
  <installSteps order="Explicit">
    <installStep name="Language">
      <optionalFileGroups order="Explicit">
        <group name="Language of the settings menu" type="SelectExactlyOne">
          <plugins order="Explicit">
$plugins
          </plugins>
        </group>
      </optionalFileGroups>
    </installStep>
  </installSteps>
  <conditionalFileInstalls>
    <patterns>
$patterns
    </patterns>
  </conditionalFileInstalls>
</config>
"@
[System.IO.File]::WriteAllText("$stage\fomod\info.xml", $info.Replace("`r`n", "`n"), $utf8)
[System.IO.File]::WriteAllText("$stage\fomod\ModuleConfig.xml", $config.Replace("`r`n", "`n"), $utf8)

# ---- the archive, with forward slashes in every entry name ----
Add-Type -AssemblyName System.IO.Compression.FileSystem
if (Test-Path $zip) { Remove-Item $zip }
$archive = [System.IO.Compression.ZipFile]::Open($zip, 'Create')
try {
    Get-ChildItem $stage -Recurse -File | ForEach-Object {
        $entry = $_.FullName.Substring($stage.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $_.FullName, $entry, 'Optimal') | Out-Null
    }
} finally { $archive.Dispose() }

# ---- read it back as an installer would ----
$problems = New-Object System.Collections.Generic.List[string]
$archive = [System.IO.Compression.ZipFile]::OpenRead($zip)
try {
    $entries = @{}
    foreach ($e in $archive.Entries) { $entries[$e.FullName] = $e }
    $read = { param($path) $s = $entries[$path].Open(); $m = New-Object System.IO.MemoryStream; $s.CopyTo($m); $s.Dispose(); $m.ToArray() }
    foreach ($x in 'fomod/ModuleConfig.xml', 'fomod/info.xml') {
        if (-not $entries.ContainsKey($x)) { $problems.Add("missing $x"); continue }
        $bytes = & $read $x
        if ($bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB) { $problems.Add("$x has a BOM") }
        $head = [System.Text.Encoding]::UTF8.GetString($bytes, 0, [Math]::Min(80, $bytes.Length))
        if ($head -notmatch '^<\?xml version="1\.0" encoding="UTF-8"\?>') { $problems.Add("$x : the declaration must use double quotes (Vortex)") }
    }
    foreach ($top in ($entries.Keys | ForEach-Object { $_.Split('/')[0] } | Sort-Object -Unique)) {
        if ($top -notin 'fomod', 'core') { $problems.Add("unexpected top-level entry: $top") }
    }
    [xml]$xml = [System.Text.Encoding]::UTF8.GetString((& $read 'fomod/ModuleConfig.xml'))
    $group = $xml.config.installSteps.installStep.optionalFileGroups.group
    if ($group.type -ne 'SelectExactlyOne') { $problems.Add("group type $($group.type)") }
    [xml]$infoXml = [System.Text.Encoding]::UTF8.GetString((& $read 'fomod/info.xml'))
    if ($infoXml.fomod.Name -ne $name) { $problems.Add("info.xml Name is '$($infoXml.fomod.Name)', not $name") }
    $flags = @($group.plugins.plugin | ForEach-Object { $_.conditionFlags.flag.'#text' })
    if ($flags.Count -ne $languages.Count) { $problems.Add("$($flags.Count) choices, $($languages.Count) languages") }
    $sets = @{}
    foreach ($p in $xml.config.conditionalFileInstalls.patterns.pattern) {
        $flag = $p.dependencies.flagDependency.value
        $source = $p.files.folder.source.Replace('\', '/')
        if ($flag -notin $flags) { $problems.Add("pattern $flag has no choice") }
        $files = @($entries.Keys | Where-Object { $_.StartsWith("$source/") } | ForEach-Object { $_.Substring($source.Length + 1) })
        if ($files.Count -eq 0) { $problems.Add("$source : no files in the archive") }
        $sets[$flag] = $files
        foreach ($need in "SKSE/Plugins/$name.dll", "$name.esp", 'COPYING.txt', 'THIRD_PARTY_NOTICES.txt', "MCM/Config/$name/config.json", "MCM/Config/$name/settings.ini", 'Scripts/SN_SexualHistory.pex', 'Scripts/SN_SexualHistoryChatter.pex', 'SKSE/Plugins/SN_SexualHistory/strings.json', 'PrismaUI/views/SN_SexualHistory/index.html', 'Magelight/SN_SexualHistory/views/index.html', 'SKSE/Plugins/SkyrimNet/external/nightlantern-mods.sn-sexual-history/settings/SNSexualMemories.yaml', 'SKSE/Plugins/SkyrimNet/external/nightlantern-mods.sn-sexual-history/prompts/sn_sexual_history_memory.prompt') {
            if ($need -notin $files) { $problems.Add("$source : missing $need") }
        }
        foreach ($g in $gameLanguages) {
            $t = "$source/Interface/Translations/${name}_$g.txt"
            if (-not $entries.ContainsKey($t)) { $problems.Add("$source : missing $t"); continue }
            $b = & $read $t
            if ($b[0] -ne 0xFF -or $b[1] -ne 0xFE) { $problems.Add("$t : not UTF-16 LE with BOM") }
            $text = [System.Text.Encoding]::Unicode.GetString($b, 2, $b.Length - 2)
            $isJapanese = $text -match '[\u3040-\u30FF]'
            if (($flag -eq 'ja') -ne $isJapanese) { $problems.Add("$t : not in the language of choice $flag") }
        }
        foreach ($f in $files) {
            if ($f -match '\.(log|tmp|bak)$') { $problems.Add("$source : stray file $f") }
        }
        if (($files | Where-Object { $_ -like '*.esp' }).Count -ne 1) { $problems.Add("$source : not exactly one plugin") }
    }
    foreach ($flag in $flags) {
        if (-not $sets.ContainsKey($flag)) { $problems.Add("choice $flag installs nothing"); continue }
        $diff = Compare-Object ($sets['en'] | Sort-Object) ($sets[$flag] | Sort-Object)
        if ($diff) { $problems.Add("choice $flag : files differ from English: $(($diff | ForEach-Object { $_.InputObject }) -join ', ')") }
    }
    $count = $sets['en'].Count
} finally { $archive.Dispose() }

if ($problems.Count) {
    $problems | ForEach-Object { Write-Host "PROBLEM: $_" -ForegroundColor Red }
    exit 1
}
"$([IO.Path]::GetFileName($zip)): $($languages.Count) languages x $count files, $([math]::Round((Get-Item $zip).Length/1KB)) KB; the installer reads back clean"
