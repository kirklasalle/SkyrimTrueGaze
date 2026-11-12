param([Parameter(Mandatory)][string]$OutputPath)
$ErrorActionPreference = 'Stop'

# Empty AS2 movie: native GFx creates the noninteractive text/marker fields.
# SWF8 RECT is 1280x720 pixels in twips; no copyrighted game UI is copied.
$bits = '10000' + ('0' * 16) + [Convert]::ToString(25600, 2).PadLeft(16, '0') +
    ('0' * 16) + [Convert]::ToString(14400, 2).PadLeft(16, '0')
$bits = $bits.PadRight([int]([Math]::Ceiling($bits.Length / 8.0) * 8), '0')
$rect = for ($i = 0; $i -lt $bits.Length; $i += 8) {
    [Convert]::ToByte($bits.Substring($i, 8), 2)
}
$body = [byte[]](@($rect) + @(0, 30, 1, 0, 64, 0, 0, 0))
$bytes = [byte[]](@(70, 87, 83, 8) + @([BitConverter]::GetBytes([uint32](8 + $body.Length))) + @($body))
New-Item -ItemType Directory -Force -Path (Split-Path $OutputPath -Parent) | Out-Null
[IO.File]::WriteAllBytes($OutputPath, $bytes)
