# Creates the multi-resolution Windows icon embedded in the app and used by
# its desktop shortcut. Uses vector drawing so it stays crisp at small sizes.
Add-Type -AssemblyName System.Drawing

$iconRoot = Join-Path $PSScriptRoot '..\resources'
$iconPath = Join-Path $iconRoot 'airplay.ico'
New-Item -ItemType Directory -Path $iconRoot -Force | Out-Null

function New-AirPlayPng([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    $rect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
    $bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect,
        [System.Drawing.Color]::FromArgb(255, 8, 20, 42),
        [System.Drawing.Color]::FromArgb(255, 24, 65, 106), 45.0)
    $radius = [Math]::Max(2.0, $size * 0.22)
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = [single]($radius * 2)
    $path.AddArc(0, 0, $d, $d, 180, 90)
    $path.AddArc(($size - $d), 0, $d, $d, 270, 90)
    $path.AddArc(($size - $d), ($size - $d), $d, $d, 0, 90)
    $path.AddArc(0, ($size - $d), $d, $d, 90, 90)
    $path.CloseFigure()
    $g.FillPath($bg, $path)

    $white = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $cyan = New-Object System.Drawing.SolidBrush(
        [System.Drawing.Color]::FromArgb(255, 75, 218, 255))
    $penWidth = [Math]::Max(1.2, $size * 0.065)
    $pen = New-Object System.Drawing.Pen([System.Drawing.Color]::White, $penWidth)
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round

    # Screen outline, floating play mark and a compact AirPlay base.
    $screen = New-Object System.Drawing.RectangleF(
        ([single]($size * 0.20)), ([single]($size * 0.17)),
        ([single]($size * 0.60)), ([single]($size * 0.48)))
    $screenRadius = [Math]::Max(1.0, $size * 0.055)
    $screenPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $sr = [single]($screenRadius * 2)
    $screenPath.AddArc($screen.X, $screen.Y, $sr, $sr, 180, 90)
    $screenPath.AddArc(($screen.Right - $sr), $screen.Y, $sr, $sr, 270, 90)
    $screenPath.AddArc(($screen.Right - $sr), ($screen.Bottom - $sr), $sr, $sr, 0, 90)
    $screenPath.AddArc($screen.X, ($screen.Bottom - $sr), $sr, $sr, 90, 90)
    $screenPath.CloseFigure()
    $g.DrawPath($pen, $screenPath)

    $triangle = [System.Drawing.PointF[]]@(
        [System.Drawing.PointF]::new([single]($size * 0.45), [single]($size * 0.30)),
        [System.Drawing.PointF]::new([single]($size * 0.45), [single]($size * 0.51)),
        [System.Drawing.PointF]::new([single]($size * 0.63), [single]($size * 0.405)))
    $g.FillPolygon($cyan, $triangle)

    $g.DrawLine($pen, [single]($size * 0.50), [single]($size * 0.67),
        [single]($size * 0.50), [single]($size * 0.77))
    $g.DrawLine($pen, [single]($size * 0.35), [single]($size * 0.81),
        [single]($size * 0.65), [single]($size * 0.81))

    $stream = New-Object System.IO.MemoryStream
    $bmp.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    $bytes = $stream.ToArray()
    $stream.Dispose()
    $pen.Dispose(); $white.Dispose(); $cyan.Dispose()
    $screenPath.Dispose(); $path.Dispose(); $bg.Dispose(); $g.Dispose(); $bmp.Dispose()
    return ,$bytes
}

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = foreach ($s in $sizes) { New-AirPlayPng $s }
$headerLength = 6 + 16 * $sizes.Count
$offset = $headerLength
$file = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter($file)
$writer.Write([uint16]0)
$writer.Write([uint16]1)
$writer.Write([uint16]$sizes.Count)
for ($i = 0; $i -lt $sizes.Count; $i++) {
    $s = $sizes[$i]
    $image = $images[$i]
    $writer.Write([byte]($(if ($s -eq 256) { 0 } else { $s })))
    $writer.Write([byte]($(if ($s -eq 256) { 0 } else { $s })))
    $writer.Write([byte]0); $writer.Write([byte]0)
    $writer.Write([uint16]1); $writer.Write([uint16]32)
    $writer.Write([uint32]$image.Length)
    $writer.Write([uint32]$offset)
    $offset += $image.Length
}
foreach ($image in $images) { $writer.Write([byte[]]$image) }
[System.IO.File]::WriteAllBytes($iconPath, $file.ToArray())
$writer.Dispose(); $file.Dispose()
Write-Output "Created $iconPath"
