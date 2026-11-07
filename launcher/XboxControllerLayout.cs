namespace ScoobyReMasked;

static class XboxControllerLayout
{
    // Action-equivalent GC inputs, not identically labelled GC face buttons.
    // Kept in sync with no100f_xbox_layout.h by XboxControllerLayoutTests.
    public static readonly (string key, string button)[] Bindings =
    {
        ("a", "south"), ("b", "west"), ("x", "east"), ("y", "north"),
        ("start", "start"), ("z", "disabled"),
        ("l", "left_trigger@50"), ("r", "right_trigger@50"),
        ("up", "dpad_up"), ("down", "dpad_down"), ("left", "dpad_left"), ("right", "dpad_right"),
    };

    public static void Apply(GameConfig config)
    {
        foreach (var (key, button) in Bindings) config.SetString("controller", key, button);
        config.SetString("controller", "xbox_black", "right_shoulder");
        config.SetString("controller", "xbox_white", "left_shoulder");
        config.SetString("controller", "layout", "xbox_original");
    }
}
