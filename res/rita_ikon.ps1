# Ikonens källa: ritar kraftledningstornet med System.Drawing (GDI+).
# Skriver ikon-256.png och ikon-32.png bredvid skriptet. Körs för hand en gång (se CLAUDE.md i res).
# Kräver bara Windows PowerShell 5.1, som finns på alla Windows-datorer.

Add-Type -AssemblyName System.Drawing

# Utmappen är skriptets katalog, annars res under aktuell katalog.
$outDir = $PSScriptRoot
if ([string]::IsNullOrEmpty($outDir)) { $outDir = Join-Path (Get-Location).Path 'res' }

# Färger: blå platta (gradient uppifrån och ned), vitt torn och ledare i bärnsten.
$blueTop = [System.Drawing.ColorTranslator]::FromHtml('#3B82C4')
$blueBottom = [System.Drawing.ColorTranslator]::FromHtml('#1E4F8C')
$white = [System.Drawing.Color]::White
$amber = [System.Drawing.ColorTranslator]::FromHtml('#FFC83D')

# Skapar en rityta med högsta kvalitet och genomskinlig bakgrund.
function New-Canvas([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([System.Drawing.Color]::Transparent)
    return @{ Bitmap = $bmp; Graphics = $g }
}

# En rundad rektangel som bana: x, y, bredd, höjd och hörnradie.
function New-RoundedRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $d = $r * 2
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

# Ett penselstreck med rundade ändar.
function New-Pen($color, [float]$width) {
    $pen = New-Object System.Drawing.Pen($color, $width)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    return $pen
}

# Fyller plattan med lodrät gradient och lämnar den som banan för klippning.
function Draw-Plate($g, [float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $path = New-RoundedRect $x $y $w $h $r
    $rect = New-Object System.Drawing.RectangleF($x, $y, $w, $h)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, $blueTop, $blueBottom, 90.0)
    $g.FillPath($brush, $path)
    $brush.Dispose()
    return $path
}

# Sparar bilden som PNG och frigör ritytan.
function Save-Canvas($canvas, [string]$name) {
    $file = Join-Path $outDir $name
    $canvas.Graphics.Dispose()
    $canvas.Bitmap.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
    $canvas.Bitmap.Dispose()
    Write-Host "Skrev $file"
}

# En hängande ledare som en kubisk Bezierkurva, klippt mot plattan.
function Draw-Conductor($g, $pen, $platePath, [float]$x0, [float]$y0, [float]$cx1, [float]$cy1, [float]$cx2, [float]$cy2, [float]$x1, [float]$y1) {
    $g.SetClip($platePath)
    $g.DrawBezier($pen, $x0, $y0, $cx1, $cy1, $cx2, $cy2, $x1, $y1)
    $g.ResetClip()
}

# ---------------------------------------------------------------- 256 px
$c = New-Canvas 256
$g = $c.Graphics
$plate = Draw-Plate $g 16 16 224 224 44

# Benens x vid höjden y: nedre delen (212 till 104) och övre delen (104 till 44).
function Leg-X([float]$y) {
    if ($y -ge 104) { return 84 + (212 - $y) * 28 / 108 }
    return 112 + (104 - $y) * 8 / 60
}

# Ledarna ligger bakom tornet. Nedre traversen bär två ledare, den övre två.
$cond = New-Pen $amber 5
Draw-Conductor $g $cond $plate 56 118 44 150 20 156 0 140
Draw-Conductor $g $cond $plate 200 118 212 150 236 156 256 140
Draw-Conductor $g $cond $plate 76 82 56 108 28 112 0 100
Draw-Conductor $g $cond $plate 180 82 200 108 228 112 256 100

# Kryssförband: tre fält, två diagonaler i varje.
$thin = New-Pen $white 5
$fields = @(@(212, 158), @(158, 104), @(104, 44))
foreach ($f in $fields) {
    $yb = $f[0]; $yt = $f[1]
    $xbl = Leg-X $yb; $xbr = 256 - $xbl
    $xtl = Leg-X $yt; $xtr = 256 - $xtl
    $g.DrawLine($thin, $xbl, $yb, $xtr, $yt)
    $g.DrawLine($thin, $xbr, $yb, $xtl, $yt)
    $g.DrawLine($thin, $xtl, $yt, $xtr, $yt)
}
$g.DrawLine($thin, (Leg-X 212), 212, 256 - (Leg-X 212), 212)

# Traverser och snedstag mellan traversernas ändar och tornkroppen.
$mid = New-Pen $white 8
$g.DrawLine($mid, 56, 104, 200, 104)
$g.DrawLine($mid, 76, 68, 180, 68)
$g.DrawLine($thin, 56, 104, (Leg-X 132), 132)
$g.DrawLine($thin, 200, 104, 256 - (Leg-X 132), 132)
$g.DrawLine($thin, 76, 68, (Leg-X 90), 90)
$g.DrawLine($thin, 180, 68, 256 - (Leg-X 90), 90)

# Benen och toppen: de bärande linjerna i kraftigare streck, jordlinan som en kort spets.
$leg = New-Pen $white 10
$g.DrawLines($leg, @(
    (New-Object System.Drawing.PointF(84, 212)),
    (New-Object System.Drawing.PointF(112, 104)),
    (New-Object System.Drawing.PointF(120, 44))))
$g.DrawLines($leg, @(
    (New-Object System.Drawing.PointF(172, 212)),
    (New-Object System.Drawing.PointF(144, 104)),
    (New-Object System.Drawing.PointF(136, 44))))
$g.DrawLine($leg, 120, 44, 136, 44)
$g.DrawLine($mid, 128, 44, 128, 28)

# Isolatorer: korta tjocka streck under traversernas ändar, där ledarna hänger.
$ins = New-Pen $white 7
$g.DrawLine($ins, 56, 104, 56, 118)
$g.DrawLine($ins, 200, 104, 200, 118)
$g.DrawLine($ins, 76, 68, 76, 82)
$g.DrawLine($ins, 180, 68, 180, 82)

Save-Canvas $c 'ikon-256.png'

# ---------------------------------------------------------------- 32 px
# Förenklad version som håller i 16 till 32 px: färre och tjockare streck.
$c = New-Canvas 32
$g = $c.Graphics
$plate = Draw-Plate $g 1 1 30 30 6

$cond = New-Pen $amber 2
Draw-Conductor $g $cond $plate 7 13 5 18 2 19 0 17
Draw-Conductor $g $cond $plate 25 13 27 18 30 19 32 17
Draw-Conductor $g $cond $plate 10 9 7 12 3 13 0 12
Draw-Conductor $g $cond $plate 22 9 25 12 29 13 32 12

$thin = New-Pen $white 1.2
$g.DrawLine($thin, 11.2, 24, 20.8, 14)
$g.DrawLine($thin, 20.8, 24, 11.2, 14)
$g.DrawLine($thin, 12.5, 19, 19.5, 19)

$mid = New-Pen $white 2
$g.DrawLine($mid, 7, 11, 25, 11)
$g.DrawLine($mid, 10, 7, 22, 7)

$leg = New-Pen $white 2
$g.DrawLine($leg, 10, 28, 15, 6)
$g.DrawLine($leg, 22, 28, 17, 6)
$g.DrawLine($leg, 10, 28, 22, 28)

Save-Canvas $c 'ikon-32.png'
