using System.Text.RegularExpressions;
using ScoobyReMasked;
using Xunit;

namespace Translator.Tests;

public sealed class XboxControllerLayoutTests
{
    [Fact]
    public void LauncherAndRuntimeHaveTheSameActionEquivalentBindings()
    {
        string header = File.ReadAllText(Path.Combine(AppContext.BaseDirectory, "no100f_xbox_layout.h"));
        string bindings = header.Split("kBindings{")[1].Split("};")[0];
        var tokens = Regex.Matches(bindings, "\"([^\"]+)\"")
            .Select(match => match.Groups[1].Value).ToArray();
        Assert.Equal(tokens, XboxControllerLayout.Bindings.Select(binding => binding.button));
        Assert.Equal(new[] { "a", "b", "x", "y", "start", "z", "l", "r", "up", "down", "left", "right" },
                     XboxControllerLayout.Bindings.Select(binding => binding.key));
    }

    [Fact]
    public void ApplyingIsExplicitAndPersistsWithoutChangingKeyboardOrOtherSettings()
    {
        string directory = Path.Combine(Path.GetTempPath(), "no100f-layout-" + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(directory);
        string path = Path.Combine(directory, "Config.toml");
        const string original = "# retained comment\n[controller]\na = \"north\"\nexpr_z = \"custom\"\n" +
                                "[keyboard]\na = \"space\"\n[video]\nshow_fps = true\n";
        try
        {
            File.WriteAllText(path, original);
            var config = new GameConfig(path);
            Assert.Null(config.Get("controller", "layout"));
            Assert.Equal(original, File.ReadAllText(path));
            XboxControllerLayout.Apply(config);
            Assert.Equal(original, File.ReadAllText(path));
            config.Save();
            var reload = new GameConfig(path);
            foreach (var (key, button) in XboxControllerLayout.Bindings)
                Assert.Equal(button, reload.Get("controller", key));
            Assert.Equal("xbox_original", reload.Get("controller", "layout"));
            Assert.Equal("right_shoulder", reload.Get("controller", "xbox_black"));
            Assert.Equal("left_shoulder", reload.Get("controller", "xbox_white"));
            Assert.Equal("space", reload.Get("keyboard", "a"));
            Assert.Equal("custom", reload.Get("controller", "expr_z"));
            Assert.Equal("true", reload.Get("video", "show_fps"));
            Assert.Contains("# retained comment", File.ReadAllText(path));
        }
        finally
        {
            File.Delete(path);
            Directory.Delete(directory);
        }
    }
}
