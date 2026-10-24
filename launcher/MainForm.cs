using System.Diagnostics;
using System.Drawing.Drawing2D;
using System.Media;

namespace ScoobyReMasked;

// The launcher window: a header of real Scooby-Doo artwork with the title, Shaggy holding a scared Scooby under the
// menu, and one card per page (Play, Display, Controls, Mods). Everything it changes goes to
// data\UserData\Config.toml, the keys the game's F10 menu uses. All art is drawn here; nothing from the game is shipped.
sealed class MainForm : Form
{
    const string GameExe = "Scooby-Doo Refrightened.exe", AppTitle = "Scooby-Doo: Refrightened";
    static Color Hex(string s) => ColorTranslator.FromHtml(s);
    // The game's palette: the logo's purple with its orange-tan rim, Mystic Manor's green sky and amber windows.
    static readonly Color Cream = Hex("#ECEFE4"), Paper = Hex("#FBFCF8"), Ink = Hex("#1F1830"), Teal = Hex("#5E3C91"),
        TealLight = Hex("#E5DCF3"), Lime = Hex("#8CC63F"), Orange = Hex("#E8963A"), Gold = Hex("#F8CB72"), Brown = Hex("#7A5232"),
        Night = Hex("#1D573C"), Dusk = Hex("#5AA57A"), Moon = Hex("#F2F7E8"), Shadow = Hex("#0E1F1A");

    // Fonts are in points and grow with Windows display scaling; every pixel size goes through S() so it grows too.
    static float k = 1f;
    static int S(int v) => (int)Math.Round(v * k);
    static Size S(int w, int h) => new(S(w), S(h));
    static Padding S(int l, int t, int r, int b) => new(S(l), S(t), S(r), S(b));

    readonly string root, play, userData, configPath, modsDir, discDir;
    readonly string? shotDir;
    readonly Panel content = new() { Dock = DockStyle.Fill };
    readonly List<(Button button, Control page)> pages = new();
    Label playStatus = null!, dumpHelp = null!;
    Button chooseDisc = null!, playButton = null!;
    ProgressBar progress = null!;

    public MainForm(string? shotDir = null)
    {
        this.shotDir = shotDir;
        k = DeviceDpi / 96f;
        (root, play) = FindGame();
        userData = Path.Combine(play, "UserData");
        configPath = Path.Combine(userData, "Config.toml");
        modsDir = Path.Combine(userData, "mods");
        // Release: the disc lives in data\Disc next to the game. Dev tree (Play\): the shared Disc\ at the root.
        discDir = IsRelease ? Path.Combine(play, "Disc", "DATA") : Path.Combine(root, "Disc", "DATA");

        Text = AppTitle;
        try { Icon = Icon.ExtractAssociatedIcon(Environment.ProcessPath!); } catch { }
        Cursor = glassCursor;
        var area = Screen.PrimaryScreen?.WorkingArea.Size ?? new Size(S(1060), S(740));
        ClientSize = new Size(Math.Min(S(1060), area.Width - 40), Math.Min(S(740), area.Height - 60));
        MinimumSize = new Size(Math.Min(S(900), area.Width), Math.Min(S(620), area.Height));
        BackColor = Cream;
        Font = new Font("Segoe UI", 10.5f);
        StartPosition = FormStartPosition.CenterScreen;
        DoubleBuffered = true;
        content.Padding = S(16, 12, 20, 16);

        var header = new BufferedPanel { Dock = DockStyle.Top, Height = S(170) };
        header.Paint += PaintHeader;
        var nav = new BufferedFlow { Dock = DockStyle.Left, Width = S(220), FlowDirection = FlowDirection.TopDown, Padding = S(16, 18, 0, 0), WrapContents = false };
        nav.Paint += PaintNav;
        var frame = new System.Windows.Forms.Timer { Interval = 33 };
        frame.Tick += (_, _) =>
        {
            if (!animate || WindowState == FormWindowState.Minimized) return;
            nav.Invalidate(new Rectangle(0, navSpriteTop, nav.Width, Math.Max(0, nav.Height - navSpriteTop)));
            float glow = (float)(Math.Sin(T * 3) + 1) / 2;  // PLAY breathes between orange and gold
            if (playButton.Enabled) playButton.BackColor = Color.FromArgb(Lerp(Orange.R, Gold.R, glow), Lerp(Orange.G, Gold.G, glow), Lerp(Orange.B, Gold.B, glow));
        };
        frame.Start();
        Controls.Add(content);
        Controls.Add(nav);
        Controls.Add(header);

        AddPage(nav, "Play", BuildPlayPage());
        AddPage(nav, "Display", BuildDisplayPage());
        AddPage(nav, "Controls", BuildControlsPage());
        AddPage(nav, "Mods", BuildModsPage());
        ShowPage(pages[0].button);
        Shown += (_, _) =>
        {
            if (shotDir == null) return;
            // Headless visual check: every page drawn to <dir>\<page>.png, plus the two pointers.
            Directory.CreateDirectory(shotDir);
            shotShift = 9;
            foreach (var p in pages)
            {
                ShowPage(p.button);
                Application.DoEvents();
                using var bmp = new Bitmap(Width, Height);
                DrawToBitmap(bmp, new Rectangle(Point.Empty, Size));
                bmp.Save(Path.Combine(shotDir, p.button.Text + ".png"));
            }
            using (var cur = new Bitmap(S(80), S(40)))
            using (var g = Graphics.FromImage(cur))
            {
                g.Clear(Cream);
                for (int i = 0; i < cursorBitmaps.Count; i++) g.DrawImageUnscaled(cursorBitmaps[i], i * S(40), 0);
                cur.Save(Path.Combine(shotDir, "cursor.png"));
            }
            Close();
        };
    }

    // The game folder: data\ beside the launcher (release), or Play\ above launcher\publish (dev tree).
    static (string root, string game) FindGame()
    {
        for (var d = new DirectoryInfo(AppContext.BaseDirectory); d != null; d = d.Parent)
            foreach (var sub in new[] { "data", "Play" })
                if (File.Exists(Path.Combine(d.FullName, sub, GameExe))) return (d.FullName, Path.Combine(d.FullName, sub));
        return (AppContext.BaseDirectory, Path.Combine(AppContext.BaseDirectory, "data"));
    }

    bool IsRelease => Path.GetFileName(play) == "data";

    void AddPage(FlowLayoutPanel nav, string title, Control page)
    {
        var button = new Button
        {
            Text = title, Size = S(188, 50), FlatStyle = FlatStyle.Flat, BackColor = Paper, ForeColor = Ink,
            Font = new Font("Segoe UI", 13f, FontStyle.Bold), TextAlign = ContentAlignment.MiddleLeft, Padding = S(12, 0, 0, 0),
            Margin = S(0, 0, 0, 10), Cursor = glassHand,
        };
        button.FlatAppearance.BorderColor = Ink;
        button.FlatAppearance.BorderSize = S(2);
        button.Click += (_, _) => ShowPage(button);
        button.MouseEnter += (_, _) => { if (button.BackColor == Paper) button.BackColor = TealLight; };
        button.MouseLeave += (_, _) => { if (button.BackColor == TealLight) button.BackColor = Paper; };
        page.Dock = DockStyle.Fill;
        page.Visible = false;
        content.Controls.Add(page);
        nav.Controls.Add(button);
        pages.Add((button, page));
    }

    void ShowPage(Button button)
    {
        foreach (var (b, page) in pages)
        {
            bool on = b == button;
            b.BackColor = on ? Teal : Paper;
            b.ForeColor = on ? Color.White : Ink;
            page.Visible = on;
        }
        RefreshDisc();
    }

    // ----- The art: header and side column -----
    // The pictures are real Scooby-Doo artwork (launcher\art); nothing here draws scenery.
    readonly bool animate = SystemInformation.UIEffectsEnabled;
    readonly Stopwatch clock = Stopwatch.StartNew();
    double T => (animate ? clock.Elapsed.TotalSeconds : 1.0) + shotShift;
    double shotShift;
    Bitmap? headerImage;
    int navSpriteTop;
    static readonly Color HeaderTeal = Hex("#00B5BC");  // the background of the header artwork

    static int Lerp(int a, int b, float f) => (int)(a + (b - a) * f);

    // Standard Windows pointers (the names are kept from the shared launcher code). Static so Btn() can use them.
    static Cursor glassCursor = Cursors.Default, glassHand = Cursors.Hand;
    readonly List<Bitmap> cursorBitmaps = new();

    // The title face: the embedded title.ttf when the build has one (a Scooby-Doo logo look-alike), else a stock face.
    static readonly System.Drawing.Text.PrivateFontCollection titleFonts = new();
    static FontFamily TitleFont()
    {
        try
        {
            using var s = typeof(MainForm).Assembly.GetManifestResourceStream("ScoobyReMasked.title.ttf");
            if (s != null && titleFonts.Families.Length == 0)
            {
                var data = ReadAll(s);
                var mem = System.Runtime.InteropServices.Marshal.AllocCoTaskMem(data.Length);  // kept for the app's life
                System.Runtime.InteropServices.Marshal.Copy(data, 0, mem, data.Length);
                titleFonts.AddMemoryFont(mem, data.Length);
            }
            if (titleFonts.Families.Length > 0) return titleFonts.Families[0];
        }
        catch { }
        foreach (var name in new[] { "Cooper Black", "Segoe UI Black" })
            if (FontFamily.Families.Any(f => f.Name == name)) return new FontFamily(name);
        return new FontFamily("Segoe UI");
    }

    // The header, drawn once per size: the artwork on the right (Shaggy and Scooby's faces), fading into its own teal
    // on the left where the title sits, in the game logo's purple with an orange-tan rim.
    void BuildHeader(Size size)
    {
        headerImage?.Dispose();
        headerImage = new Bitmap(size.Width, size.Height);
        using var g = Graphics.FromImage(headerImage);
        g.SmoothingMode = SmoothingMode.AntiAlias;
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        float W = size.Width, H = size.Height;
        using (var teal = new SolidBrush(HeaderTeal)) g.FillRectangle(teal, 0, 0, W, H);
        if (HeaderArt != null)
        {
            // Cover-crop the faces (source x 160..1920, centred on y 420) into the right 62% of the header.
            var dest = new RectangleF(W * 0.38f, 0, W * 0.62f, H);
            float srcW = HeaderArt.Width - 160f, srcH = srcW * dest.Height / dest.Width;
            if (srcH > HeaderArt.Height) { srcH = HeaderArt.Height; srcW = srcH * dest.Width / dest.Height; }
            float srcY = Math.Clamp(420f - srcH / 2, 0, HeaderArt.Height - srcH);
            g.DrawImage(HeaderArt, dest, new RectangleF(HeaderArt.Width - srcW, srcY, srcW, srcH), GraphicsUnit.Pixel);
            // Blend the cut edge into the teal.
            var fade = new RectangleF(dest.X - 1, 0, S(110), H);
            using var blend = new LinearGradientBrush(fade, HeaderTeal, Color.FromArgb(0, HeaderTeal), LinearGradientMode.Horizontal);
            blend.WrapMode = WrapMode.TileFlipX;  // the default Tile repeats the gradient for a pixel at its far edge
            g.FillRectangle(blend, fade);
        }
        g.TextRenderingHint = System.Drawing.Text.TextRenderingHint.AntiAliasGridFit;
        var face = TitleFont();
        var style = (int)(face.IsStyleAvailable(FontStyle.Bold) ? FontStyle.Bold : FontStyle.Regular);
        float em = Math.Min(S(46), W * 0.045f), left = S(22);
        using (var path = new GraphicsPath())
        {
            path.AddString("SCOOBY-DOO!", face, style, em, new PointF(left, S(16)), StringFormat.GenericDefault);
            using (var outline = new Pen(Hex("#1A0F24"), S(9)) { LineJoin = LineJoin.Round }) g.DrawPath(outline, path);
            using (var rim = new Pen(Hex("#E8A04A"), S(4)) { LineJoin = LineJoin.Round }) g.DrawPath(rim, path);
            using var fill = new LinearGradientBrush(new RectangleF(0, S(18), 10, em), Hex("#A55FD0"), Hex("#5C2A8A"), LinearGradientMode.Vertical);
            g.FillPath(fill, path);
        }
        using (var path = new GraphicsPath())
        {
            path.AddString("Refrightened", face, style, em * 0.66f, new PointF(left + em * 0.45f, S(16) + em * 1.08f), StringFormat.GenericDefault);
            using (var outline = new Pen(Hex("#1A0F24"), S(7)) { LineJoin = LineJoin.Round }) g.DrawPath(outline, path);
            using var fill = new SolidBrush(Hex("#FFF4C8"));
            g.FillPath(fill, path);
        }
        using var credit = new Font("Segoe UI", 10f, FontStyle.Bold);
        using var ink = new SolidBrush(Hex("#1A0F24"));
        g.DrawString("Made by JosiahG", credit, ink, left + em * 0.5f, S(16) + em * 1.9f);
    }

    void PaintHeader(object? sender, PaintEventArgs e)
    {
        var r = ((Control)sender!).ClientRectangle;
        if (r.Width <= 0 || r.Height <= 0) return;
        if (headerImage == null || headerImage.Size != r.Size) BuildHeader(r.Size);
        e.Graphics.DrawImageUnscaled(headerImage!, 0, 0);
    }

    // The artwork (embedded from launcher\art), resized once per size and kept.
    static readonly Image? HeaderArt = LoadArt("header.jpg"), ScoobyArt = LoadArt("shaggy_scooby.png");
    readonly Dictionary<int, Bitmap> scoobySized = new();

    static Image? LoadArt(string name)
    {
        using var s = typeof(MainForm).Assembly.GetManifestResourceStream("ScoobyReMasked." + name);
        return s == null ? null : Image.FromStream(new MemoryStream(ReadAll(s)));
    }

    static byte[] ReadAll(Stream s) { using var m = new MemoryStream(); s.CopyTo(m); return m.ToArray(); }

    static Bitmap SizedArt(Image art, int height)
    {
        var b = new Bitmap(Math.Max(1, art.Width * height / art.Height), Math.Max(1, height));
        using var g = Graphics.FromImage(b);
        g.InterpolationMode = InterpolationMode.HighQualityBicubic;
        g.PixelOffsetMode = PixelOffsetMode.HighQuality;
        g.DrawImage(art, new Rectangle(Point.Empty, b.Size));
        return b;
    }

    // Shaggy holding a scared Scooby under the menu, with a slow nervous bob.
    void PaintNav(object? sender, PaintEventArgs e)
    {
        var nav = (Control)sender!;
        var g = e.Graphics;
        navSpriteTop = pages.Count > 0 ? pages[^1].button.Bottom + S(10) : 0;
        int bottom = nav.ClientSize.Height - S(8);
        int room = bottom - navSpriteTop;
        if (room < S(80) || ScoobyArt == null) return;
        int height = Math.Min(S(300), room);
        if (!scoobySized.TryGetValue(height, out var art)) scoobySized[height] = art = SizedArt(ScoobyArt, height);
        int cx = (nav.Width + S(16)) / 2;
        int lift = (int)(Math.Abs(Math.Sin(T * 2.2)) * S(3));
        g.DrawImageUnscaled(art, cx - art.Width / 2, bottom - art.Height - lift);
    }

    // ----- Shared pieces -----
    // A white card holding one page's controls, top to bottom.
    static (Panel card, FlowLayoutPanel body) NewCard(string heading)
    {
        var card = new BufferedPanel { Padding = S(22, 16, 22, 16) };
        card.Paint += (sender, e) =>
        {
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            var r = ((Control)sender!).ClientRectangle;
            r.Width -= S(8); r.Height -= S(8);
            r.Offset(S(2), S(2));
            using var path = Rounded(r, S(14));
            using var shadow = Rounded(new Rectangle(r.X + S(5), r.Y + S(5), r.Width, r.Height), S(14));
            using (var shade = new SolidBrush(Color.FromArgb(50, 0, 0, 0))) g.FillPath(shade, shadow);
            using (var paper = new SolidBrush(Paper)) g.FillPath(paper, path);
            using (var border = new Pen(Ink, S(3))) g.DrawPath(border, path);
        };
        var body = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, AutoSizeMode = AutoSizeMode.GrowAndShrink, FlowDirection = FlowDirection.TopDown, WrapContents = false, BackColor = Paper };
        card.AutoScroll = true;
        body.Controls.Add(new Label { Text = heading, Font = new Font("Segoe UI", 18f, FontStyle.Bold), ForeColor = Ink, AutoSize = true, Margin = S(0, 0, 0, 10) });
        card.Controls.Add(body);
        return (card, body);
    }

    static GraphicsPath Rounded(Rectangle r, int radius)
    {
        var p = new GraphicsPath();
        int d = Math.Max(2, radius * 2);
        p.AddArc(r.X, r.Y, d, d, 180, 90);
        p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
        p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90);
        p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
        p.CloseFigure();
        return p;
    }

    static Label Note(string text, int width = 680) => new()
    {
        Text = text, AutoSize = true, MaximumSize = new Size(S(width), 0), ForeColor = Ink, Margin = S(0, 6, 0, 6),
    };

    static Button Btn(string text, EventHandler onClick, Color? back = null)
    {
        var b = new Button
        {
            Text = text, AutoSize = true, FlatStyle = FlatStyle.Flat, BackColor = back ?? Gold, ForeColor = Ink,
            Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Padding = S(10, 4, 10, 4), Margin = S(0, 6, 10, 6), Cursor = glassHand,
        };
        b.FlatAppearance.BorderColor = Ink;
        b.FlatAppearance.BorderSize = S(2);
        b.Click += onClick;
        return b;
    }

    // ----- Play -----
    Control BuildPlayPage()
    {
        var (card, body) = NewCard("Ready, gang?");
        playButton = Btn("PLAY", (_, _) => Launch(), Orange);
        playButton.Font = new Font("Segoe UI", 24f, FontStyle.Bold);
        playButton.Padding = S(60, 8, 60, 8);
        body.Controls.Add(playButton);
        playStatus = Note("");
        playStatus.Font = new Font("Segoe UI", 10.5f, FontStyle.Bold);
        body.Controls.Add(playStatus);
        chooseDisc = Btn("Choose disc image (.iso, .gcm, .ciso)...", async (_, _) => await ChooseDisc(), Paper);
        body.Controls.Add(chooseDisc);
        progress = new ProgressBar { Width = S(520), Height = S(16), Visible = false, Margin = S(0, 4, 0, 6) };
        body.Controls.Add(progress);
        body.Controls.Add(dumpHelp = Note(
            "Scooby-Doo: Refrightened runs your own copy of Scooby-Doo! Night of 100 Frights for the GameCube (USA, GIHE78, version 1.01). " +
            "Dump the disc you own (for example with CleanRip on a Wii) as a plain .iso or .ciso, then choose it above. " +
            "The launcher copies the game files into the Disc folder once; the image file isn't needed afterwards."));
        body.Controls.Add(Note("In the game, F10 opens the settings menu (resolution, picture shape, controls, audio) and F11 switches between fullscreen and a window."));
        body.Controls.Add(BuildHolidayRow());
        // Scooby's tips: click the bubble for another one.
        var tipRow = new FlowLayoutPanel { AutoSize = true, Margin = S(0, 10, 0, 6) };
        if (ScoobyArt != null)
        {
            var pic = SizedArt(ScoobyArt, S(60));
            tipRow.Controls.Add(new PictureBox { Image = pic, Size = pic.Size, Margin = S(0, 0, 8, 0) });
        }
        tipLabel = new Label
        {
            AutoSize = true, MaximumSize = new Size(S(600), 0), BackColor = TealLight, ForeColor = Ink,
            BorderStyle = BorderStyle.FixedSingle, Padding = S(10, 8, 10, 8), Cursor = glassHand, Margin = S(0, 4, 0, 0),
        };
        tipLabel.Click += (_, _) => NextTip();
        tipRow.Controls.Add(tipLabel);
        tipIndex = Random.Shared.Next(Tips.Length) - 1;
        NextTip();
        body.Controls.Add(tipRow);
        var row = new FlowLayoutPanel { AutoSize = true, Margin = Padding.Empty };
        row.Controls.Add(Btn("Open game folder", (_, _) => Process.Start("explorer.exe", play), Paper));
        row.Controls.Add(Btn("Open saves and settings", (_, _) => Process.Start("explorer.exe", Directory.CreateDirectory(userData).FullName), Paper));
        body.Controls.Add(row);
        return card;
    }

    // The hubs' holiday events ([game] holiday): the game checks the date when a hub loads. "auto" uses today's date,
    // "off" none, "MM-DD" pretends it is that day.
    static readonly (string text, string value)[] Holidays =
    {
        ("By today's date", "auto"),
        ("Off", "off"),
        ("New Year's Day", "01-01"),
        ("St. Patrick's Day", "03-17"),
        ("Fourth of July", "07-04"),
        ("Halloween", "10-31"),
        ("Christmas", "12-25"),
    };

    Control BuildHolidayRow()
    {
        var row = new FlowLayoutPanel { AutoSize = true, Margin = S(0, 6, 0, 6), BackColor = Paper };
        row.Controls.Add(new Label { Text = "Holidays", AutoSize = true, ForeColor = Ink, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Margin = S(0, 8, 16, 0) });
        var box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = S(360), Cursor = glassHand };
        foreach (var h in Holidays) box.Items.Add(h.text);
        var current = new GameConfig(configPath).Get("game", "holiday") ?? "auto";
        int index = Array.FindIndex(Holidays, h => h.value == current);
        if (index < 0) { box.Items.Add("Custom date (" + current + ")"); index = box.Items.Count - 1; }
        box.SelectedIndex = index;
        box.SelectedIndexChanged += (_, _) =>
        {
            if (box.SelectedIndex >= Holidays.Length) return;
            var c = new GameConfig(configPath);
            c.SetString("game", "holiday", Holidays[box.SelectedIndex].value);
            c.Save();
        };
        row.Controls.Add(box);
        var wrap = new FlowLayoutPanel { AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Margin = Padding.Empty, BackColor = Paper };
        wrap.Controls.Add(row);
        wrap.Controls.Add(Note("Outside Mystic Manor and in the playground, the game celebrates holidays on their real dates. " +
                               "Pick one to see it any day, or turn them off."));
        return wrap;
    }

    static readonly string[] Tips =
    {
        "Press F10 in the game to change resolution, picture shape or controls without quitting.",
        "F11 flips the game between fullscreen and a window.",
        "Cinematic 2.39:1 shows far more of Mystic Manor than the GameCube ever could.",
        "Any gamepad works as a GameCube controller right away.",
        "Got an HDR screen? Switch HDR on in Windows, then on the Display page.",
        "Saves live in data\\UserData. A backup now and then never hurts.",
        "Scooby's on his own this time. Like, good luck, Scoob!",
    };
    Label? tipLabel;
    int tipIndex;

    void NextTip()
    {
        tipIndex = (tipIndex + 1) % Tips.Length;
        if (tipLabel != null) tipLabel.Text = "Scooby says: " + Tips[tipIndex];
    }

    (bool ok, string text) DiscState()
    {
        var boot = Path.Combine(discDir, "sys", "boot.bin");
        if (!File.Exists(boot)) return (false, "No game disc set up yet. Choose your Scooby-Doo! Night of 100 Frights disc image below.");
        var id = System.Text.Encoding.ASCII.GetString(File.ReadAllBytes(boot), 0, 6);
        return id == GcDisc.WantedId ? (true, "Scooby-Doo! Night of 100 Frights (GIHE78) is ready. Zoinks, let's go!") : (false, $"The Disc folder holds {id}, not Scooby-Doo! Night of 100 Frights USA (GIHE78).");
    }

    void RefreshDisc()
    {
        if (playStatus == null) return;
        var (ok, text) = DiscState();
        playStatus.Text = text;
        playStatus.ForeColor = ok ? Hex("#1E7F55") : Hex("#B4561A");
        playButton.Enabled = ok && File.Exists(Path.Combine(play, GameExe));
        if (!playButton.Enabled) playButton.BackColor = Hex("#D8CFC0");
        chooseDisc.Text = ok ? "Change disc image..." : "Choose disc image (.iso, .gcm, .ciso)...";
        dumpHelp.Visible = !ok;
    }

    async Task ChooseDisc()
    {
        using var dialog = new OpenFileDialog
        {
            Title = "Choose your Scooby-Doo! Night of 100 Frights disc image",
            Filter = "GameCube disc images (*.iso;*.gcm;*.ciso)|*.iso;*.gcm;*.ciso|All files (*.*)|*.*",
        };
        if (dialog.ShowDialog(this) != DialogResult.OK) return;
        GcDisc disc;
        try { disc = GcDisc.Open(dialog.FileName); }
        catch (Exception e) { MessageBox.Show(this, "That file can't be read: " + e.Message, AppTitle); return; }
        using (disc)
        {
            var problem = disc.Problem();
            if (problem.Length > 0) { MessageBox.Show(this, problem, AppTitle, MessageBoxButtons.OK, MessageBoxIcon.Warning); return; }
            var staging = discDir + ".new";
            if (Directory.Exists(staging)) Directory.Delete(staging, true);
            chooseDisc.Enabled = playButton.Enabled = false;
            progress.Visible = true; progress.Value = 0;
            var report = new Progress<(int done, int total, string name)>(p =>
            {
                progress.Maximum = Math.Max(1, p.total); progress.Value = Math.Min(p.done, progress.Maximum);
                playStatus.Text = $"Extracting {p.done}/{p.total}: {p.name}";
            });
            try
            {
                await Task.Run(() => disc.Extract(staging, (d, t, n) => ((IProgress<(int, int, string)>)report).Report((d, t, n))));
                if (Directory.Exists(discDir)) Directory.Delete(discDir, true);
                Directory.Move(staging, discDir);
                var cfg = new GameConfig(configPath);
                cfg.Set("paths", "dvd_root", IsRelease ? "'../Disc/DATA'" : "'../../Disc/DATA'");  // relative to UserData, so the folder can move
                cfg.Save();
            }
            catch (Exception e) { MessageBox.Show(this, "Extraction failed: " + e.Message, AppTitle, MessageBoxButtons.OK, MessageBoxIcon.Error); }
            finally { progress.Visible = false; chooseDisc.Enabled = true; RefreshDisc(); }
        }
    }

    void Launch()
    {
        var exe = Path.Combine(play, GameExe);
        if (!File.Exists(exe)) { MessageBox.Show(this, "The game files (the data folder) are missing. Re-extract the download next to this launcher.", AppTitle); return; }
        Process.Start(new ProcessStartInfo(exe) { WorkingDirectory = play, UseShellExecute = true });
        WindowState = FormWindowState.Minimized;
    }

    // ----- Display -----
    Control BuildDisplayPage()
    {
        var (card, body) = NewCard("Display");
        var cfg = new GameConfig(configPath);
        var table = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, BackColor = Paper, Margin = S(0, 4, 0, 8) };
        void Row(string label, string key, (string text, string raw)[] options, string fallback)
        {
            var box = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = S(360), Cursor = glassHand };
            foreach (var o in options) box.Items.Add(o.text);
            var current = cfg.Get("video", key) ?? fallback;
            int index = Array.FindIndex(options, o => o.raw.Trim('"') == current.Trim('"'));
            box.SelectedIndex = index >= 0 ? index : Array.FindIndex(options, o => o.raw.Trim('"') == fallback);
            box.SelectedIndexChanged += (_, _) =>
            {
                var c = new GameConfig(configPath);
                c.Set("video", key, options[box.SelectedIndex].raw);
                c.Save();
            };
            table.Controls.Add(new Label { Text = label, AutoSize = true, ForeColor = Ink, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Margin = S(0, 10, 16, 8) });
            table.Controls.Add(box);
        }
        Row("Window", "display_mode", new[] { ("Borderless fullscreen", "\"borderless\""), ("Window", "\"windowed\""), ("Exclusive fullscreen", "\"exclusive\"") }, "borderless");
        Row("Resolution", "resolution_multiplier", new[] { ("720p", "1.5"), ("1080p", "2.25"), ("4K", "4.5"), ("Your screen's size", "0") }, "2.25");
        Row("Picture shape", "aspect_mode", new[] { ("Original 4:3", "0"), ("Widescreen 16:9", "1"), ("Fill the screen", "2"), ("Cinematic 2.39:1", "3") }, "3");
        Row("Frame rate", "fps60", new[] { ("30 fps (original)", "false"), ("60 fps", "true") }, "false");
        Row("HD textures", "texture_replacements", new[] { ("On", "true"), ("Off (original textures)", "false") }, "true");
        Row("Anti-aliasing", "msaa", new[] { ("Off", "1"), ("2x MSAA", "2"), ("4x MSAA", "4"), ("8x MSAA", "8") }, "1");
        Row("Intro movies", "skip_intro", new[] { ("Play them", "false"), ("Skip them", "true") }, "false");
        Row("HDR", "hdr", new[] { ("Off", "false"), ("On (needs HDR on in Windows display settings)", "true") }, "false");
        Row("HDR brightness", "hdr_paper_white_nits", new[] { ("Dim (120 nits)", "120.0"), ("Normal (200 nits)", "200.0"), ("Bright (300 nits)", "300.0"), ("Brightest (400 nits)", "400.0") }, "200.0");
        Row("HDR highlights", "hdr_peak_nits", new[] { ("Subtle (400 nits)", "400.0"), ("Standard (1000 nits)", "1000.0"), ("Strong (1500 nits)", "1500.0") }, "1000.0");
        body.Controls.Add(table);
        body.Controls.Add(Note("Changes are saved at once and apply the next time the game starts. In the game, F10 opens the same " +
                               "settings and F11 switches between fullscreen and a window."));
        return card;
    }

    // ----- Controls -----
    Control BuildControlsPage()
    {
        var (card, body) = NewCard("Controls");
        body.Controls.Add(Note("Gamepads (Xbox, PlayStation, Switch Pro, GameCube adapter...) work right away as a GameCube controller. " +
                               "To change any button, press F10 in the game and open Controller settings."));
        body.Controls.Add(new Label { Text = "Keyboard", Font = new Font("Segoe UI", 13f, FontStyle.Bold), ForeColor = Ink, AutoSize = true, Margin = S(0, 10, 0, 0) });
        var table = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, BackColor = Paper, Margin = S(0, 8, 0, 8), CellBorderStyle = TableLayoutPanelCellBorderStyle.Single };
        foreach (var (what, key) in new[] { ("Move", "W A S D"), ("Camera (C-stick)", "Arrow keys"), ("A (jump)", "Space"), ("B", "Left Shift"),
                     ("X / Y", "R / F"), ("Z", "C"), ("L / R", "Q / E"), ("Start (pause)", "Enter"), ("D-pad", "I J K L") })
        {
            table.Controls.Add(new Label { Text = what, AutoSize = true, ForeColor = Ink, Margin = S(8, 6, 24, 6) });
            table.Controls.Add(new Label { Text = key, AutoSize = true, ForeColor = Ink, Font = new Font("Segoe UI", 10.5f, FontStyle.Bold), Margin = S(8, 6, 24, 6) });
        }
        body.Controls.Add(table);
        return card;
    }

    // ----- Mods -----
    Control BuildModsPage()
    {
        var (card, body) = NewCard("Mods");
        body.Controls.Add(Note("A mod is a folder in data\\UserData\\mods. Its files\\ folder mirrors the disc's files\\ (replacing game files), " +
                               "and its textures\\ folder holds texture replacements. Untick a mod to switch it off; when two mods change the same file, the later name wins."));
        var list = new CheckedListBox { Width = S(520), Height = S(220), CheckOnClick = true, BorderStyle = BorderStyle.FixedSingle, Margin = S(0, 8, 0, 8) };
        void Fill()
        {
            list.Items.Clear();
            if (!Directory.Exists(modsDir)) return;
            foreach (var d in Directory.GetDirectories(modsDir).OrderBy(d => d, StringComparer.OrdinalIgnoreCase))
                list.Items.Add(Path.GetFileName(d), !File.Exists(Path.Combine(d, "disabled")));
        }
        list.ItemCheck += (_, e) =>
        {
            var flag = Path.Combine(modsDir, (string)list.Items[e.Index], "disabled");
            if (e.NewValue == CheckState.Checked) File.Delete(flag); else File.WriteAllText(flag, "");
        };
        body.Controls.Add(list);
        var row = new FlowLayoutPanel { AutoSize = true, Margin = Padding.Empty };
        row.Controls.Add(Btn("Open the mods folder", (_, _) => Process.Start("explorer.exe", Directory.CreateDirectory(modsDir).FullName), Paper));
        row.Controls.Add(Btn("Refresh", (_, _) => Fill(), Paper));
        body.Controls.Add(row);
        Fill();
        return card;
    }
}

sealed class BufferedPanel : Panel
{
    public BufferedPanel() { DoubleBuffered = true; ResizeRedraw = true; }
}

sealed class BufferedFlow : FlowLayoutPanel
{
    public BufferedFlow() { DoubleBuffered = true; ResizeRedraw = true; }
}
