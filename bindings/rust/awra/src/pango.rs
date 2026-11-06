//! Text layout enums (Pango names).

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum EllipsizeMode {
    #[default]
    None,
    Start,
    Middle,
    End,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum WrapMode {
    #[default]
    Word,
    Char,
    WordChar,
}
