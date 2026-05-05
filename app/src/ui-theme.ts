/**
 * Centralised UI colour + spacing tokens for Murphy Mate.
 *
 * NOT a full theme system (we use react-navigation's built-in
 * ThemeProvider for light/dark scaffolding). This is just a single
 * source of truth for the literal hex values that previously appeared
 * inline in every screen, so we can rename "primary blue" without
 * grepping all of `app/`.
 */

export const Palette = {
  // Brand / action colours.
  primary: '#007AFF',      // primary CTA, links, active chips
  success: '#34C759',      // save / confirm / "use this folder"
  warning: '#FF9500',      // edit-settings CTA, loading dot
  danger: '#FF3B30',       // delete / destructive / unbind
  progress: '#4CD964',     // upload progress fill (slightly different green)

  // Neutrals.
  white: '#fff',
  black: '#000',
  textPrimary: '#333',
  textSecondary: '#666',
  textTertiary: '#888',
  textMuted: '#999',
  textError: '#c44',
  textInverse: '#fff',

  // Surfaces.
  bg: '#fff',
  bgMuted: '#f0f0f0',
  bgPanel: '#f0f4f8',
  bgPickerHeader: '#f5f5f5',
  bgEditor: '#f7f9fc',
  border: '#ddd',
  borderLight: '#eee',
  scrim: 'rgba(0,0,0,0.4)',

  // Disabled state.
  disabled: '#A0A0A0',

  // Status dot fallback (for the "idle / unknown" connection state).
  neutralDot: '#A0A0A0',
} as const;

/**
 * Lightweight spacing scale. Most screens hard-code 8/12/16/20px;
 * keeping a named scale makes future audits easier without forcing a
 * full design-system overhaul.
 */
export const Spacing = {
  xs: 4,
  sm: 8,
  md: 12,
  lg: 16,
  xl: 20,
  xxl: 30,
} as const;

export const Radii = {
  pill: 16,
  card: 10,
  button: 8,
  chip: 16,
} as const;
