# VHS demo for Muuk
# Generates an animated terminal GIF using charmbracelet/vhs

Output muuk_demo.gif

Require zsh
Require starship
Require muuk

Set Shell "zsh"
Set FontSize 16
Set FontFamily "JetBrainsMono Nerd Font"
Set Width 1600
Set Height 900
Set Padding 20
Set Margin 20
Set MarginFill "#000000"
Set BorderRadius 20
Set LetterSpacing 1

Hide
Type "source ~/.zshrc && clear" Enter
Show

Type "muuk -v" Sleep 500ms Enter Sleep 1s

Type "muuk init" Sleep 500ms Enter Sleep 1s
Type "myproject" Sleep 500ms Enter Sleep 1s
Type "me" Sleep 500ms Enter Sleep 1s
Type "0.0.1" Sleep 500ms Enter Sleep 1s
Enter Sleep 1s
Enter Sleep 1s
Enter Sleep 1s
Type "yes" Sleep 500ms Enter Sleep 1s

Type "muuk build" Sleep 500ms Enter Sleep 3s
Type "echo 'Build complete!'" Sleep 500ms Enter Sleep 2s