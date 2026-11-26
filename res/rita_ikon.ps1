# Ikonens källa: tre fackverksstolpar i perspektiv mot en kvällshimmel över grön mark.
# Varje stolpe har tre traverser och sex linor; varje lina går till samma fästpunkt på nästa stolpe.
# Skriver ikon-256.png och ikon-32.png bredvid skriptet. Körs för hand en gång (se CLAUDE.md i res).
# Kräver bara Windows PowerShell 5.1, som finns på alla Windows-datorer.

Add-Type -AssemblyName System.Drawing

# Utmappen är skriptets katalog, annars res under aktuell katalog.
$outDir = $PSScriptRoot
if ([string]::IsNullOrEmpty($outDir)) { $outDir = Join-Path (Get-Location).Path 'res' }

# En färg ur hex med valfri alfa.
function Get-Color([string]$hex, [int]$alpha = 255) {
    $c = [System.Drawing.ColorTranslator]::FromHtml($hex)
    return [System.Drawing.Color]::FromArgb($alpha, $c.R, $c.G, $c.B)
}

# Skapar en rityta med genomskinlig bakgrund. Ritningen görs alltid i 256 enheter
# och skalas till bildens storlek.
function New-Canvas([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $g.Clear([System.Drawing.Color]::Transparent)
    $g.ScaleTransform($size / 256.0, $size / 256.0)
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

# Ett penselstreck med rundade ändar. Färgen kan vara en färg eller en pensel.
function New-Pen($color, [float]$width) {
    $pen = New-Object System.Drawing.Pen($color, $width)
    $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Round
    $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
    return $pen
}

# Stolpens halva bredd vid höjden h (stolpen är 190 enheter hög): kroppen smalnar
# upp till traverserna och sedan mot spetsen.
function Get-HalfWidth([float]$h) {
    if ($h -le 96) { return 44 - $h * 28 / 96 }
    if ($h -le 160) { return 16 - ($h - 96) * 5 / 64 }
    return 11 - ($h - 160) * 11 / 28
}

# Traverserna som (höjd, halv längd), nedifrån, och isolatorernas längd.
$arms = @(@(96, 58), @(128, 62), @(160, 56))
$insulator = 15

# Perspektivet: flyktpunkten (Vx, Vy) på horisonten och den närmaste stolpen på djupet 1.
# En punkt i den närmaste stolpens plan flyttas till djupet z genom att skalas mot flyktpunkten.
$Vx = 4.0; $Vy = 196.0
$cx0 = 170.0; $b0 = 214.0; $s0 = 0.95

# Stolparna som (djup, sidoförskjutning, fackverkets tjocklek): förgrund, mitten och bak.
# Ledningen vinklar vid mittstolpen, så att den bakre står nästan vid vänsterkanten.
$towers = @(@(1.0, 0.0, 1.3), @(2.0, 0.0, 1.3), @(3.6, -72.0, 1.0))

# Linornas nedhäng mitt i spannet, i den närmaste stolpens plan.
$sag = 34

# Skärmpunkten för punkten (x, y) i den närmaste stolpens plan, flyttad till djupet z.
function Get-Projected([float]$x, [float]$y, [float]$z) {
    return New-Object System.Drawing.PointF(($Vx + ($x - $Vx) / $z), ($Vy + ($y - $Vy) / $z))
}

# Ritar en stolpe på djupet z, förskjuten xoff i sidled. Stålet är mörkt med en svag ljusreflex.
# Strecken skalar med roten ur storleken; k förtjockar dem i små bilder, braceK fackverket.
# detail ritar fackverket, som utelämnas i 32 px.
function Draw-Tower($g, [float]$z, [float]$xoff, [float]$k, [bool]$detail, [float]$braceK) {
    $s = $s0 / $z
    $cx = $Vx + ($cx0 - $Vx + $xoff) / $z
    $by = $Vy + ($b0 - $Vy) / $z
    $rect = New-Object System.Drawing.RectangleF(($cx - 64 * $s), ($by - 192 * $s), (128 * $s), (194 * $s))
    $steel = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, (Get-Color '#2A3644'), (Get-Color '#1C2530'), 0.0)
    $blend = New-Object System.Drawing.Drawing2D.ColorBlend(4)
    $blend.Colors = @((Get-Color '#2A3644'), (Get-Color '#3E4C5C'), (Get-Color '#2F3B49'), (Get-Color '#1C2530'))
    $blend.Positions = @(0.0, 0.38, 0.6, 1.0)
    $steel.InterpolationColors = $blend
    $w = { param($base) [Math]::Max(0.514 * $base * [Math]::Sqrt($s) * $k, 0.9) }
    $leg = New-Pen $steel (& $w 7)
    $arm = New-Pen $steel (& $w 5)
    $brace = New-Pen $steel (& $w (2.6 * $braceK))
    $ins = New-Pen $steel (& $w 4)
    $X = { param($dx) $cx + $dx * $s }
    $Y = { param($h) $by - $h * $s }

    # Benen från foten till spetsen.
    foreach ($sgn in @(-1, 1)) {
        $pts = @(0, 96, 160, 188) | ForEach-Object {
            New-Object System.Drawing.PointF((& $X ($sgn * (Get-HalfWidth $_))), (& $Y $_))
        }
        $g.DrawLines($leg, $pts)
    }

    # Kryssförband och tvärslåar i kroppens fält, och en slå vid foten.
    if ($detail) {
        $levels = @(0, 34, 66, 96, 128, 160)
        for ($i = 0; $i -lt $levels.Count - 1; $i++) {
            $lo = $levels[$i]; $hi = $levels[$i + 1]
            $g.DrawLine($brace, (& $X (-(Get-HalfWidth $lo))), (& $Y $lo), (& $X (Get-HalfWidth $hi)), (& $Y $hi))
            $g.DrawLine($brace, (& $X (Get-HalfWidth $lo)), (& $Y $lo), (& $X (-(Get-HalfWidth $hi))), (& $Y $hi))
            $g.DrawLine($brace, (& $X (-(Get-HalfWidth $hi))), (& $Y $hi), (& $X (Get-HalfWidth $hi)), (& $Y $hi))
        }
        $g.DrawLine($brace, (& $X (-44)), (& $Y 0), (& $X 44), (& $Y 0))
    }

    # Jordlinespetsen överst.
    $g.DrawLine($arm, (& $X (-16)), (& $Y 182), (& $X 16), (& $Y 182))

    # Traverserna: underram vågrät, överram snett från kroppen, isolatorn nedåt från änden.
    foreach ($a in $arms) {
        $h = $a[0]; $len = $a[1]
        foreach ($sgn in @(-1, 1)) {
            $tip = & $X ($sgn * $len)
            $g.DrawLine($arm, (& $X ($sgn * (Get-HalfWidth $h))), (& $Y $h), $tip, (& $Y $h))
            if ($detail) {
                $g.DrawLine($brace, (& $X ($sgn * (Get-HalfWidth ($h + 12)))), (& $Y ($h + 12)), $tip, (& $Y $h))
            }
            $g.DrawLine($ins, $tip, (& $Y $h), $tip, (& $Y ($h - $insulator)))
        }
    }
}

# Ritar ett spann: de sex linorna, var och en från sin fästpunkt till samma fästpunkt på nästa
# stolpe. Stolparna står på djupen z1 och z2 med sidoförskjutningarna x1 och x2. Linan hänger
# som en parabel i rummet. t0 och t1 begränsar spannet (0 till 1 är hela).
function Draw-Span($g, $color, [float]$z1, [float]$x1, [float]$z2, [float]$x2, [float]$w0, [float]$t0, [float]$t1) {
    foreach ($a in $arms) {
        foreach ($sgn in @(-1, 1)) {
            $ax = $cx0 + $sgn * $a[1] * $s0
            $ay = $b0 - ($a[0] - $insulator) * $s0
            $pts = @()
            for ($i = 0; $i -le 48; $i++) {
                $t = $t0 + ($t1 - $t0) * $i / 48
                $z = $z1 + $t * ($z2 - $z1)
                $x = $ax + $x1 + $t * ($x2 - $x1)
                $pts += Get-Projected $x ($ay + 4 * $sag * $t * (1 - $t)) $z
            }
            $zm = $z1 + ($z2 - $z1) * ($t0 + $t1) / 2
            $g.DrawLines((New-Pen $color ([Math]::Max($w0 / $zm, 0.8))), $pts)
        }
    }
}

# Ritar hela ikonen i den givna storleken: himmel, ljus vid horisonten, mark, stolpar och linor.
# Under 64 px utelämnas fackverket och strecken förtjockas. GDI+ ritar streck smalare än en
# pixel som en hel pixel, så i små bilder tonas linorna i stället.
function Draw-Icon([int]$size) {
    $small = $size -lt 64
    $k = if ($small) { 2.4 } else { 1.0 }
    $lineA = if ($small) { 0.35 } else { 1.0 }
    $c = New-Canvas $size
    $g = $c.Graphics

    # Plattan med himmel, mörk upptill och ljus mot horisonten.
    $plate = New-RoundedRect 8 8 240 240 48
    $rect = New-Object System.Drawing.RectangleF(8, 8, 240, 240)
    $sky = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect, (Get-Color '#16406F'), (Get-Color '#9CC7EE'), 90.0)
    $g.FillPath($sky, $plate)
    $g.SetClip($plate)

    # Varmt ljus vid horisonten.
    $glow = New-Object System.Drawing.Drawing2D.GraphicsPath
    $glow.AddEllipse(-60, 140, 240, 110)
    $glowBrush = New-Object System.Drawing.Drawing2D.PathGradientBrush($glow)
    $glowBrush.CenterColor = Get-Color '#FFD9A0' 170
    $glowBrush.SurroundColors = @((Get-Color '#FFD9A0' 0))
    $g.FillPath($glowBrush, $glow)

    # Marken i grönt: mörkare vid horisonten, ljusare fram.
    $groundRect = New-Object System.Drawing.RectangleF(0, 199, 256, 57)
    $ground = New-Object System.Drawing.Drawing2D.LinearGradientBrush($groundRect, (Get-Color '#1F4D2B'), (Get-Color '#7CC05A'), 90.0)
    $g.FillRectangle($ground, 0, 199, 256, 57)

    # Stolparna bakifrån och fram. Spannet till den bortre stolpen ritas efter den, före den närmare.
    for ($i = $towers.Count - 1; $i -ge 0; $i--) {
        $z = $towers[$i][0]; $xo = $towers[$i][1]
        Draw-Tower $g $z $xo $k (-not $small) $towers[$i][2]
        if ($i -ge 1) {
            # Linorna bleknar något med avståndet.
            $prev = $towers[$i - 1]
            $alpha = [int](255 - ($z - 1) * 22)
            $lineColor = Get-Color '#FFFFFF' ([int]([Math]::Max($alpha - 10, 60) * $lineA))
            Draw-Span $g $lineColor $prev[0] $prev[1] $z $xo 1.2 0 1
        }
    }

    # Spannet mot betraktaren: från förra stolpen (bakom kameran) fram till den närmaste.
    Draw-Span $g (Get-Color '#FFFFFF' ([int](255 * $lineA))) 0 0 1 0 1.2 0.45 1

    $g.ResetClip()
    return $c
}

# Sparar bilden som PNG och frigör ritytan.
function Save-Canvas($canvas, [string]$name) {
    $file = Join-Path $outDir $name
    $canvas.Graphics.Dispose()
    $canvas.Bitmap.Save($file, [System.Drawing.Imaging.ImageFormat]::Png)
    $canvas.Bitmap.Dispose()
    Write-Host "Skrev $file"
}

Save-Canvas (Draw-Icon 256) 'ikon-256.png'
Save-Canvas (Draw-Icon 32) 'ikon-32.png'
