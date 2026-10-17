$ErrorActionPreference = 'Stop'
$manualDir = Join-Path $PSScriptRoot '_local/manual'
New-Item -ItemType Directory -Path $manualDir -Force | Out-Null
$base = 'http://oslab.mobisys.top/lab2026/_book/lab2/'
$index = (curl.exe -fsSL --connect-timeout 10 --max-time 30 ($base + 'lab2.html')) -join "`n"
if ($LASTEXITCODE -ne 0) { throw 'Cannot fetch Lab2 index' }
$pages = @('lab2.html') + @([regex]::Matches($index, 'href="(lab2[^"]+\.html)"') | ForEach-Object { $_.Groups[1].Value } | Select-Object -Unique)
foreach ($page in $pages) {
    $htmlPath = Join-Path $manualDir $page
    curl.exe -fsSL --connect-timeout 10 --max-time 30 ($base + $page) -o $htmlPath
    if ($LASTEXITCODE -ne 0) { throw "Cannot fetch $page" }
    $html = Get-Content -Raw -Encoding utf8 -LiteralPath $htmlPath
    $section = [regex]::Match($html, '(?s)<section class="normal markdown-section">(.*?)</section>').Groups[1].Value
    $plain = [System.Net.WebUtility]::HtmlDecode([regex]::Replace($section, '<[^>]+>', ''))
    $plain | Set-Content -Encoding utf8 -LiteralPath (Join-Path $manualDir ($page + '.txt'))
    Write-Output $page
}
