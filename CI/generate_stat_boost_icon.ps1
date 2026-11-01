Add-Type -AssemblyName System.Drawing
$bitmap = New-Object System.Drawing.Bitmap 16, 16
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.Clear([System.Drawing.Color]::Transparent)
$outline = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 30, 24, 10))
$gold = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 255, 216, 80))
try {
    foreach ($offset in @(0, 7)) {
        $outer = [System.Drawing.Point[]]@(
            [System.Drawing.Point]::new(1, 5 + $offset),
            [System.Drawing.Point]::new(8, 0 + $offset),
            [System.Drawing.Point]::new(15, 5 + $offset),
            [System.Drawing.Point]::new(15, 9 + $offset),
            [System.Drawing.Point]::new(8, 4 + $offset),
            [System.Drawing.Point]::new(1, 9 + $offset)
        )
        $inner = [System.Drawing.Point[]]@(
            [System.Drawing.Point]::new(2, 5 + $offset),
            [System.Drawing.Point]::new(8, 1 + $offset),
            [System.Drawing.Point]::new(14, 5 + $offset),
            [System.Drawing.Point]::new(14, 7 + $offset),
            [System.Drawing.Point]::new(8, 3 + $offset),
            [System.Drawing.Point]::new(2, 7 + $offset)
        )
        $graphics.FillPolygon($outline, $outer)
        $graphics.FillPolygon($gold, $inner)
    }
    $destination = Join-Path $PSScriptRoot '../Mods/vcmi/Content/Sprites/mapView/statBoost.png'
    $null = New-Item -ItemType Directory -Force -Path (Split-Path $destination)
    $bitmap.Save([System.IO.Path]::GetFullPath($destination), [System.Drawing.Imaging.ImageFormat]::Png)
} finally {
    $gold.Dispose()
    $outline.Dispose()
    $graphics.Dispose()
    $bitmap.Dispose()
}
