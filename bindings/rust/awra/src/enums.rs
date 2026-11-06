//! Enumerations, with Awra 1 / GTK names.

macro_rules! c_enum {
    ($(#[$m:meta])* $name:ident { $($v:ident = $n:expr),+ $(,)? } default $d:ident) => {
        $(#[$m])*
        #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
        #[repr(i32)]
        pub enum $name { $($v = $n),+ }
        impl Default for $name {
            fn default() -> Self { $name::$d }
        }
    };
}

c_enum!(Orientation { Horizontal = 0, Vertical = 1 } default Horizontal);
c_enum!(Align { Fill = 0, Start = 1, End = 2, Center = 3, Baseline = 4 } default Fill);
c_enum!(Spacing { None = -1, Xs = 0, Sm = 1, Md = 2, Lg = 3, Xl = 4, Xxl = 5 } default Md);
c_enum!(Radius { Small = 0, Medium = 1, Large = 2, Window = 3, Pill = 4 } default Medium);
c_enum!(Typography {
    Body = 0, Muted = 1, Eyebrow = 2, Title3 = 3, Title2 = 4, Title1 = 5, Display = 6,
    Monospace = 7, Label = 8, Reading = 9
} default Body);
#[allow(non_upper_case_globals)]
impl Typography {
    /// Awra 1's "title" role (Title 2).
    pub const Title: Typography = Typography::Title2;
}
c_enum!(Appearance { System = 0, Light = 1, Dark = 2 } default System);
c_enum!(AccentSource { Default = 0, System = 1, Custom = 2 } default Default);
c_enum!(ButtonAppearance { Primary = 0, Secondary = 1, Ghost = 2, Destructive = 3, Toolbar = 4 } default Secondary);
c_enum!(CardAppearance { Plain = 0, Tinted = 1, Raised = 2 } default Plain);
c_enum!(BadgeAppearance { Neutral = 0, Accent = 1, Success = 2, Warning = 3, Danger = 4, Info = 5 } default Neutral);
c_enum!(StatusAppearance { Info = 0, Success = 1, Warning = 2, Error = 3 } default Info);
c_enum!(Material {
    Canvas = 0, Content = 1, Chrome = 2, Layer = 3, Floating = 4, Opaque = 5, Solid = 6,
    Translucent = 7, Frosted = 8
} default Content);
c_enum!(Elevation { Flat = 0, Raised = 1, Floating = 2, Modal = 3 } default Flat);
c_enum!(MotionPreset { Instant = 0, Fast = 1, Normal = 2, Slow = 3, Emphasized = 4 } default Normal);
c_enum!(PolicyType { Automatic = 0, Always = 1, Never = 2, External = 3 } default Automatic);
c_enum!(Justification { Left = 0, Center = 1, Right = 2, Fill = 3 } default Left);
c_enum!(StackTransitionType { None = 0, Crossfade = 1, SlideLeftRight = 2, SlideUpDown = 3 } default None);
c_enum!(SelectionMode { None = 0, Single = 1, Browse = 2, Multiple = 3 } default Single);
c_enum!(PhysicalLayout { Ansi = 0, Iso = 1 } default Ansi);
c_enum!(PositionType { Bottom = 0, Top = 1, Left = 2, Right = 3 } default Bottom);
c_enum!(ContentFit { Fill = 0, Contain = 1, Cover = 2, ScaleDown = 3 } default Contain);

/// Awra 1 alias.
pub type KeyboardPhysicalLayout = PhysicalLayout;
