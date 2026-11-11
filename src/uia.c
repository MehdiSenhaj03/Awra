/* Awra 2 on UI Automation (Windows): the accessibility tree as UIA
 * providers (IRawElementProviderSimple / Fragment / FragmentRoot and the
 * control patterns: Invoke, Toggle, Value, RangeValue, SelectionItem,
 * Selection, ExpandCollapse), answered from WM_GETOBJECT.
 *
 * Costs nothing until a client asks: the window host hands WM_GETOBJECT
 * to Awra; the first UiaRootObjectId request loads UIAutomationCore.dll
 * (and oleaut32.dll for BSTRs and SAFEARRAYs), turns the model on and
 * returns the window's root provider. Providers are made as clients reach
 * widgets, live in the widgets' snapshots and are disconnected when the
 * widgets go. Events (focus, property changes, structure, window opened /
 * closed, notifications) are raised only while UIA says clients listen.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later */

#ifdef _WIN32

#include "a11y.h"

#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <uiautomationcore.h>
#include <uiautomationcoreapi.h>
#include <uiautomationclient.h>

#include <stdio.h>

#ifdef _MSC_VER
/* The Windows SDK declares the UIA ids as `const long` variables in C,
 * which are not constant expressions: give the ones `case` labels use
 * their values (as MinGW's headers have them). WINBOOL is MinGW's BOOL. */
typedef BOOL WINBOOL;
#define UIA_AcceleratorKeyPropertyId 30006
#define UIA_AutomationIdPropertyId 30011
#define UIA_ClassNamePropertyId 30012
#define UIA_ControlTypePropertyId 30003
#define UIA_ExpandCollapsePatternId 10005
#define UIA_FrameworkIdPropertyId 30024
#define UIA_HasKeyboardFocusPropertyId 30008
#define UIA_HeadingLevelPropertyId 30173
#define UIA_HelpTextPropertyId 30013
#define UIA_InvokePatternId 10000
#define UIA_IsContentElementPropertyId 30017
#define UIA_IsControlElementPropertyId 30016
#define UIA_IsDialogPropertyId 30174
#define UIA_IsEnabledPropertyId 30010
#define UIA_IsKeyboardFocusablePropertyId 30009
#define UIA_IsOffscreenPropertyId 30022
#define UIA_IsPasswordPropertyId 30019
#define UIA_IsRequiredForFormPropertyId 30025
#define UIA_ItemStatusPropertyId 30026
#define UIA_LocalizedControlTypePropertyId 30004
#define UIA_NamePropertyId 30005
#define UIA_NativeWindowHandlePropertyId 30020
#define UIA_OrientationPropertyId 30023
#define UIA_ProcessIdPropertyId 30002
#define UIA_RangeValuePatternId 10003
#define UIA_SelectionItemPatternId 10010
#define UIA_SelectionPatternId 10001
#define UIA_TogglePatternId 10015
#define UIA_ValuePatternId 10002
#endif

/* ======================================================================= */
/* Loaded at run time */

typedef LRESULT(WINAPI *PFN_ReturnRaw)(HWND, WPARAM, LPARAM, IRawElementProviderSimple *);
typedef HRESULT(WINAPI *PFN_HostFromHwnd)(HWND, IRawElementProviderSimple **);
typedef HRESULT(WINAPI *PFN_RaiseEvent)(IRawElementProviderSimple *, EVENTID);
typedef HRESULT(WINAPI *PFN_RaiseProp)(IRawElementProviderSimple *, PROPERTYID, VARIANT, VARIANT);
typedef HRESULT(WINAPI *PFN_RaiseStructure)(IRawElementProviderSimple *, enum StructureChangeType,
                                            int *, int);
typedef HRESULT(WINAPI *PFN_RaiseNotification)(IRawElementProviderSimple *, enum NotificationKind,
                                               enum NotificationProcessing, BSTR, BSTR);
typedef HRESULT(WINAPI *PFN_Disconnect)(IRawElementProviderSimple *);
typedef BOOL(WINAPI *PFN_Listening)(void);
typedef BSTR(WINAPI *PFN_SysAlloc)(const OLECHAR *);
typedef void(WINAPI *PFN_SysFree)(BSTR);
typedef SAFEARRAY *(WINAPI *PFN_SACreateVector)(VARTYPE, LONG, ULONG);
typedef HRESULT(WINAPI *PFN_SAPut)(SAFEARRAY *, LONG *, void *);

static struct {
  bool tried, ok;
  PFN_ReturnRaw ReturnRawElementProvider;
  PFN_HostFromHwnd HostProviderFromHwnd;
  PFN_RaiseEvent RaiseAutomationEvent;
  PFN_RaiseProp RaisePropertyChanged;
  PFN_RaiseStructure RaiseStructureChanged;
  PFN_RaiseNotification RaiseNotification;
  PFN_Disconnect DisconnectProvider;
  PFN_Listening ClientsAreListening;
  PFN_SysAlloc SysAllocString;
  PFN_SysFree SysFreeString;
  PFN_SACreateVector SafeArrayCreateVector;
  PFN_SAPut SafeArrayPutElement;
} U;

static bool uia_load(void)
{
  if (U.tried) {
    return U.ok;
  }
  U.tried = true;
  HMODULE core = LoadLibraryW(L"UIAutomationCore.dll");
  HMODULE aut = LoadLibraryW(L"oleaut32.dll");
  if (!core || !aut) {
    return false;
  }
#define SYM(lib, field, name) U.field = (void *)GetProcAddress(lib, name)
  SYM(core, ReturnRawElementProvider, "UiaReturnRawElementProvider");
  SYM(core, HostProviderFromHwnd, "UiaHostProviderFromHwnd");
  SYM(core, RaiseAutomationEvent, "UiaRaiseAutomationEvent");
  SYM(core, RaisePropertyChanged, "UiaRaiseAutomationPropertyChangedEvent");
  SYM(core, RaiseStructureChanged, "UiaRaiseStructureChangedEvent");
  SYM(core, RaiseNotification, "UiaRaiseNotificationEvent"); /* Windows 10 1709+ */
  SYM(core, DisconnectProvider, "UiaDisconnectProvider");
  SYM(core, ClientsAreListening, "UiaClientsAreListening");
  SYM(aut, SysAllocString, "SysAllocString");
  SYM(aut, SysFreeString, "SysFreeString");
  SYM(aut, SafeArrayCreateVector, "SafeArrayCreateVector");
  SYM(aut, SafeArrayPutElement, "SafeArrayPutElement");
#undef SYM
  U.ok = U.ReturnRawElementProvider && U.HostProviderFromHwnd && U.RaiseAutomationEvent &&
         U.RaisePropertyChanged && U.SysAllocString && U.SysFreeString &&
         U.SafeArrayCreateVector && U.SafeArrayPutElement;
  return U.ok;
}

static bool listening(void)
{
  return U.ok && (!U.ClientsAreListening || U.ClientsAreListening());
}

/* ======================================================================= */
/* Interface ids (copied: no INITGUID clash with libuuid) */

static const GUID g_iid_unknown = {0x00000000, 0x0000, 0x0000, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};
static const GUID g_iid_simple = {0xd6dd68d1, 0x86fd, 0x4332, {0x86, 0x66, 0x9a, 0xbe, 0xde, 0xa2, 0xd2, 0x4c}};
static const GUID g_iid_fragment = {0xf7063da8, 0x8359, 0x439c, {0x92, 0x97, 0xbb, 0xc5, 0x29, 0x9a, 0x7d, 0x87}};
static const GUID g_iid_root = {0x620ce2a5, 0xab8f, 0x40a9, {0x86, 0xcb, 0xde, 0x3c, 0x75, 0x59, 0x9b, 0x58}};
static const GUID g_iid_invoke = {0x54fcb24b, 0xe18e, 0x47a2, {0xb4, 0xd3, 0xec, 0xcb, 0xe7, 0x75, 0x99, 0xa2}};
static const GUID g_iid_toggle = {0x56d00bd0, 0xc4f4, 0x433c, {0xa8, 0x36, 0x1a, 0x52, 0xa5, 0x7e, 0x08, 0x92}};
static const GUID g_iid_value = {0xc7935180, 0x6fb3, 0x4201, {0xb1, 0x74, 0x7d, 0xf7, 0x3a, 0xdb, 0xf6, 0x4a}};
static const GUID g_iid_range = {0x36dc7aef, 0x33e6, 0x4691, {0xaf, 0xe1, 0x2b, 0xe7, 0x27, 0x4b, 0x3d, 0x33}};
static const GUID g_iid_selitem = {0x2acad808, 0xb2d4, 0x452d, {0xa4, 0x07, 0x91, 0xff, 0x1a, 0xd1, 0x67, 0xb2}};
static const GUID g_iid_selection = {0xfb8b03af, 0x3bdf, 0x48d4, {0xbd, 0x36, 0x1a, 0x65, 0x79, 0x3b, 0xe1, 0x68}};
static const GUID g_iid_expand = {0xd847d3a5, 0xcab0, 0x4a98, {0x8c, 0x32, 0xec, 0xb4, 0x5c, 0x59, 0xad, 0x24}};

static bool iid_is(REFIID a, const GUID *b)
{
  return memcmp(a, b, sizeof(GUID)) == 0;
}

/* ======================================================================= */
/* Providers */

typedef struct Prov {
  const IRawElementProviderSimpleVtbl *simple; /* first: the object's identity */
  const IRawElementProviderFragmentVtbl *fragment;
  const IRawElementProviderFragmentRootVtbl *root;
  const IInvokeProviderVtbl *invoke;
  const IToggleProviderVtbl *toggle;
  const IValueProviderVtbl *value;
  const IRangeValueProviderVtbl *range;
  const ISelectionItemProviderVtbl *selitem;
  const ISelectionProviderVtbl *selection;
  const IExpandCollapseProviderVtbl *expand;
  LONG refs;
  AwraWidget *w; /* NULL: gone */
  int item;      /* -1: the widget itself */
  uint32_t id;
  HWND hwnd;     /* windows (fragment roots) */
} Prov;

#define PROV_OF(ptr, field) ((Prov *)((char *)(ptr) - offsetof(Prov, field)))

static const IRawElementProviderSimpleVtbl g_simple_vtbl;
static const IRawElementProviderFragmentVtbl g_fragment_vtbl;
static const IRawElementProviderFragmentRootVtbl g_root_vtbl;
static const IInvokeProviderVtbl g_invoke_vtbl;
static const IToggleProviderVtbl g_toggle_vtbl;
static const IValueProviderVtbl g_value_vtbl;
static const IRangeValueProviderVtbl g_range_vtbl;
static const ISelectionItemProviderVtbl g_selitem_vtbl;
static const ISelectionProviderVtbl g_selection_vtbl;
static const IExpandCollapseProviderVtbl g_expand_vtbl;

/* A popover open in a native window of its own (a menu's tool window):
 * that window (UIA sees it as its own fragment root, as Windows' menus
 * are), else NULL. */
static HWND popup_hwnd(AwraWidget *w)
{
  BKContext *ctx = w ? bk_ctx(w) : NULL;
  const int slot = ctx ? bk_popup_index(ctx, w) : -1;
  if (slot < 0 || !bk_popup_surfaced(ctx, slot) || !ctx->popup_host || !ctx->popup_host->handle) {
    return NULL;
  }
  return (HWND)ctx->popup_host->handle(ctx->popup_host->data, slot);
}

/* The native window a provider stands for (fragment roots), else NULL. */
static HWND prov_hwnd(const struct Prov *p);

static Prov *prov_alloc(AwraWidget *w, int item)
{
  Prov *p = (Prov *)calloc(1, sizeof(Prov));
  if (!p) {
    return NULL;
  }
  p->simple = &g_simple_vtbl;
  p->fragment = &g_fragment_vtbl;
  p->root = &g_root_vtbl;
  p->invoke = &g_invoke_vtbl;
  p->toggle = &g_toggle_vtbl;
  p->value = &g_value_vtbl;
  p->range = &g_range_vtbl;
  p->selitem = &g_selitem_vtbl;
  p->selection = &g_selection_vtbl;
  p->expand = &g_expand_vtbl;
  p->refs = 1;
  p->w = w;
  p->item = item;
  p->id = awra_a11y_expose(w);
  if (item < 0 && awra_widget_is(w, "AwraWindow")) {
    p->hwnd = (HWND)awra_window_native_handle(w); /* a fragment root */
  }
  return p;
}

static HWND prov_hwnd(const Prov *p)
{
  if (p->hwnd) {
    return p->hwnd;
  }
  return p->item < 0 ? popup_hwnd(p->w) : NULL;
}

static ULONG prov_addref(Prov *p)
{
  return (ULONG)InterlockedIncrement(&p->refs);
}

static ULONG prov_release(Prov *p)
{
  const LONG r = InterlockedDecrement(&p->refs);
  if (r == 0) {
    free(p);
  }
  return (ULONG)r;
}

/* the widget's own provider (kept in its snapshot); items: a new one */
static Prov *prov_get(AwraWidget *w, int item)
{
  if (!w) {
    return NULL;
  }
  if (item >= 0) {
    return prov_alloc(w, item);
  }
  awra_a11y_expose(w);
  AwraA11ySnap *s = awra_a11y_snap(w);
  if (!s) {
    return NULL;
  }
  if (!s->platform) {
    s->platform = prov_alloc(w, -1);
  }
  Prov *p = (Prov *)s->platform;
  if (p) {
    prov_addref(p);
  }
  return p;
}

static IRawElementProviderSimple *as_simple(Prov *p)
{
  return (IRawElementProviderSimple *)&p->simple;
}

static IRawElementProviderFragment *as_fragment(Prov *p)
{
  return (IRawElementProviderFragment *)&p->fragment;
}

static bool alive(const Prov *p)
{
  return p && p->w && (p->item < 0 || p->item < awra_a11y_n_items(p->w));
}

/* ----------------------------------------------------------------------- */
/* Info */

typedef struct Info {
  AwraA11yInfo i;
  double rect[4];
  unsigned flat;
} Info;

static void prov_info(const Prov *p, Info *in)
{
  if (p->item >= 0) {
    awra_a11y_item_info(p->w, p->item, &in->i, in->rect);
    in->flat = awra_a11y_item_states(p->w, &in->i);
  }
  else {
    awra_a11y_info(p->w, &in->i);
    awra_a11y_bounds(p->w, in->rect);
    in->flat = awra_a11y_states(p->w, &in->i);
  }
}

static BSTR bstr_utf8(const char *s)
{
  s = s ? s : "";
  const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  WCHAR stack[256];
  WCHAR *w = n <= 256 ? stack : (WCHAR *)malloc(sizeof(WCHAR) * (size_t)n);
  if (!w) {
    return U.SysAllocString(L"");
  }
  MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
  BSTR b = U.SysAllocString(w);
  if (w != stack) {
    free(w);
  }
  return b;
}

static char *utf8_wide(const WCHAR *w)
{
  const int n = WideCharToMultiByte(CP_UTF8, 0, w ? w : L"", -1, NULL, 0, NULL, NULL);
  char *s = (char *)malloc((size_t)n + 1);
  if (s) {
    WideCharToMultiByte(CP_UTF8, 0, w ? w : L"", -1, s, n, NULL, NULL);
  }
  return s;
}

static long control_type(const AwraA11yInfo *i)
{
  switch (i->role) {
    case AWRA_ACCESSIBLE_ROLE_BUTTON:
    case AWRA_ACCESSIBLE_ROLE_TOGGLE_BUTTON:
    case AWRA_ACCESSIBLE_ROLE_SWITCH:
      return UIA_ButtonControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_CHECKBOX: return UIA_CheckBoxControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_COMBO_BOX: return UIA_ComboBoxControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_TEXT_BOX:
    case AWRA_ACCESSIBLE_ROLE_SEARCH_BOX:
    case AWRA_ACCESSIBLE_ROLE_INPUT:
      return UIA_EditControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_LINK: return UIA_HyperlinkControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_IMG: return UIA_ImageControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_LIST_ITEM:
    case AWRA_ACCESSIBLE_ROLE_OPTION:
      return UIA_ListItemControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_LIST:
    case AWRA_ACCESSIBLE_ROLE_LIST_BOX:
      return UIA_ListControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_MENU: return UIA_MenuControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_MENU_BAR: return UIA_MenuBarControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_MENU_ITEM:
    case AWRA_ACCESSIBLE_ROLE_MENU_ITEM_CHECKBOX:
    case AWRA_ACCESSIBLE_ROLE_MENU_ITEM_RADIO:
      return UIA_MenuItemControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR:
    case AWRA_ACCESSIBLE_ROLE_METER:
      return UIA_ProgressBarControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_RADIO: return UIA_RadioButtonControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_SCROLLBAR: return UIA_ScrollBarControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_SLIDER: return UIA_SliderControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON: return UIA_SpinnerControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_STATUS: return UIA_StatusBarControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_TAB_LIST: return UIA_TabControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_TAB: return UIA_TabItemControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_LABEL:
    case AWRA_ACCESSIBLE_ROLE_HEADING:
    case AWRA_ACCESSIBLE_ROLE_PARAGRAPH:
    case AWRA_ACCESSIBLE_ROLE_CAPTION:
      return UIA_TextControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_TOOLBAR: return UIA_ToolBarControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_TOOLTIP: return UIA_ToolTipControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_TREE: return UIA_TreeControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_TREE_ITEM: return UIA_TreeItemControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_GRID:
    case AWRA_ACCESSIBLE_ROLE_TREE_GRID:
      return UIA_DataGridControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_GRID_CELL:
    case AWRA_ACCESSIBLE_ROLE_CELL:
    case AWRA_ACCESSIBLE_ROLE_ROW:
      return UIA_DataItemControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_TABLE: return UIA_TableControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_COLUMN_HEADER:
    case AWRA_ACCESSIBLE_ROLE_ROW_HEADER:
      return UIA_HeaderItemControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_DOCUMENT:
    case AWRA_ACCESSIBLE_ROLE_ARTICLE:
      return UIA_DocumentControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_SEPARATOR: return UIA_SeparatorControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_WINDOW:
    case AWRA_ACCESSIBLE_ROLE_DIALOG:
    case AWRA_ACCESSIBLE_ROLE_ALERT_DIALOG:
    case AWRA_ACCESSIBLE_ROLE_APPLICATION:
      return UIA_WindowControlTypeId;
    case AWRA_ACCESSIBLE_ROLE_GENERIC:
    case AWRA_ACCESSIBLE_ROLE_TAB_PANEL:
      return UIA_PaneControlTypeId;
    default:
      return UIA_GroupControlTypeId;
  }
}

static bool has_toggle(const Info *in)
{
  return (in->flat & AWRA_A11Y_CHECKABLE) ||
         (in->i.defined & (1u << AWRA_ACCESSIBLE_STATE_PRESSED)) ||
         in->i.role == AWRA_ACCESSIBLE_ROLE_SWITCH || in->i.role == AWRA_ACCESSIBLE_ROLE_TOGGLE_BUTTON;
}

static bool has_text_value(const Info *in)
{
  return in->i.text && (in->i.role == AWRA_ACCESSIBLE_ROLE_TEXT_BOX ||
                        in->i.role == AWRA_ACCESSIBLE_ROLE_SEARCH_BOX ||
                        in->i.role == AWRA_ACCESSIBLE_ROLE_SPIN_BUTTON);
}

static bool has_selection(const Prov *p, const Info *in)
{
  if (p->item >= 0) {
    return false;
  }
  if (in->i.selection) {
    return true;
  }
  switch (in->i.role) {
    case AWRA_ACCESSIBLE_ROLE_LIST:
    case AWRA_ACCESSIBLE_ROLE_LIST_BOX:
    case AWRA_ACCESSIBLE_ROLE_TAB_LIST:
    case AWRA_ACCESSIBLE_ROLE_GRID:
    case AWRA_ACCESSIBLE_ROLE_RADIO_GROUP:
      return true;
    default:
      return false;
  }
}

/* ----------------------------------------------------------------------- */
/* IUnknown, shared by every interface of a provider */

static HRESULT prov_qi(Prov *p, REFIID riid, void **out)
{
  *out = NULL;
  if (!p) {
    return E_NOINTERFACE;
  }
  Info in;
  const bool ok = alive(p);
  if (ok) {
    prov_info(p, &in);
  }
  void *r = NULL;
  if (iid_is(riid, &g_iid_unknown) || iid_is(riid, &g_iid_simple)) {
    r = &p->simple;
  }
  else if (iid_is(riid, &g_iid_fragment)) {
    r = &p->fragment;
  }
  else if (iid_is(riid, &g_iid_root) && prov_hwnd(p)) {
    r = &p->root;
  }
  else if (!ok) {
    return E_NOINTERFACE;
  }
  else if (iid_is(riid, &g_iid_invoke) && in.i.action && !has_toggle(&in)) {
    r = &p->invoke;
  }
  else if (iid_is(riid, &g_iid_toggle) && has_toggle(&in)) {
    r = &p->toggle;
  }
  else if (iid_is(riid, &g_iid_value) && (has_text_value(&in) || in.i.role == AWRA_ACCESSIBLE_ROLE_COMBO_BOX)) {
    r = &p->value;
  }
  else if (iid_is(riid, &g_iid_range) && in.i.has_value) {
    r = &p->range;
  }
  else if (iid_is(riid, &g_iid_selitem) && (in.flat & AWRA_A11Y_SELECTABLE)) {
    r = &p->selitem;
  }
  else if (iid_is(riid, &g_iid_selection) && has_selection(p, &in)) {
    r = &p->selection;
  }
  else if (iid_is(riid, &g_iid_expand) && (in.flat & AWRA_A11Y_EXPANDABLE)) {
    r = &p->expand;
  }
  if (!r) {
    return E_NOINTERFACE;
  }
  prov_addref(p);
  *out = r;
  return S_OK;
}

#define UNKNOWN_METHODS(iface, field)                                                   \
  static HRESULT STDMETHODCALLTYPE field##_qi(iface *This, REFIID riid, void **out)     \
  {                                                                                     \
    return prov_qi(PROV_OF(This, field), riid, out);                                    \
  }                                                                                     \
  static ULONG STDMETHODCALLTYPE field##_addref(iface *This)                            \
  {                                                                                     \
    return prov_addref(PROV_OF(This, field));                                           \
  }                                                                                     \
  static ULONG STDMETHODCALLTYPE field##_release(iface *This)                           \
  {                                                                                     \
    return prov_release(PROV_OF(This, field));                                          \
  }

UNKNOWN_METHODS(IRawElementProviderSimple, simple)
UNKNOWN_METHODS(IRawElementProviderFragment, fragment)
UNKNOWN_METHODS(IRawElementProviderFragmentRoot, root)
UNKNOWN_METHODS(IInvokeProvider, invoke)
UNKNOWN_METHODS(IToggleProvider, toggle)
UNKNOWN_METHODS(IValueProvider, value)
UNKNOWN_METHODS(IRangeValueProvider, range)
UNKNOWN_METHODS(ISelectionItemProvider, selitem)
UNKNOWN_METHODS(ISelectionProvider, selection)
UNKNOWN_METHODS(IExpandCollapseProvider, expand)

#define GONE ((HRESULT)0x80040201L) /* UIA_E_ELEMENTNOTAVAILABLE */
#define CHECK_ALIVE(p) \
  if (!alive(p)) {     \
    return GONE;       \
  }

/* ----------------------------------------------------------------------- */
/* IRawElementProviderSimple */

static HRESULT STDMETHODCALLTYPE simple_options(IRawElementProviderSimple *This,
                                                enum ProviderOptions *r)
{
  *r = ProviderOptions_ServerSideProvider;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE simple_pattern(IRawElementProviderSimple *This, PATTERNID id,
                                                IUnknown **r)
{
  Prov *p = PROV_OF(This, simple);
  *r = NULL;
  CHECK_ALIVE(p);
  const GUID *iid = NULL;
  switch (id) {
    case UIA_InvokePatternId: iid = &g_iid_invoke; break;
    case UIA_TogglePatternId: iid = &g_iid_toggle; break;
    case UIA_ValuePatternId: iid = &g_iid_value; break;
    case UIA_RangeValuePatternId: iid = &g_iid_range; break;
    case UIA_SelectionItemPatternId: iid = &g_iid_selitem; break;
    case UIA_SelectionPatternId: iid = &g_iid_selection; break;
    case UIA_ExpandCollapsePatternId: iid = &g_iid_expand; break;
    default: return S_OK;
  }
  void *out = NULL;
  if (SUCCEEDED(prov_qi(p, iid, &out))) {
    *r = (IUnknown *)out;
  }
  return S_OK;
}

/* VariantInit lives in oleaut32: done by hand */
static void v_init(VARIANT *v)
{
  memset(v, 0, sizeof(*v));
  v->vt = VT_EMPTY;
}

static void v_bool(VARIANT *v, bool b)
{
  v->vt = VT_BOOL;
  v->boolVal = b ? VARIANT_TRUE : VARIANT_FALSE;
}

static void v_str(VARIANT *v, const char *s)
{
  v->vt = VT_BSTR;
  v->bstrVal = bstr_utf8(s);
}

static void v_i4(VARIANT *v, long n)
{
  v->vt = VT_I4;
  v->lVal = n;
}

static HRESULT STDMETHODCALLTYPE simple_property(IRawElementProviderSimple *This, PROPERTYID id,
                                                 VARIANT *r)
{
  Prov *p = PROV_OF(This, simple);
  v_init(r);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  switch (id) {
    case UIA_ControlTypePropertyId:
      v_i4(r, control_type(&in.i));
      break;
    case UIA_NamePropertyId: {
      char *n = p->item >= 0 ? awra_a11y_item_name(p->w, p->item) : awra_a11y_name(p->w);
      v_str(r, n);
      free(n);
      break;
    }
    case UIA_HelpTextPropertyId: {
      char *d = p->item >= 0 ? awra_strdup(in.i.description ? in.i.description : "") :
                               awra_a11y_description(p->w);
      if (d && d[0]) {
        v_str(r, d);
      }
      free(d);
      break;
    }
    case UIA_LocalizedControlTypePropertyId: {
      const char *rd = p->item < 0 ?
                           awra_a11y_property_string(p->w, AWRA_ACCESSIBLE_PROPERTY_ROLE_DESCRIPTION) :
                           NULL;
      if (rd) {
        v_str(r, rd);
      }
      break;
    }
    case UIA_AutomationIdPropertyId: {
      const char *n = p->item < 0 ? awra_widget_name(p->w) : NULL;
      if (n) {
        v_str(r, n);
      }
      break;
    }
    case UIA_ClassNamePropertyId:
      v_str(r, awra_widget_type(p->w));
      break;
    case UIA_FrameworkIdPropertyId:
      v_str(r, "Awra");
      break;
    case UIA_IsEnabledPropertyId:
      v_bool(r, (in.flat & AWRA_A11Y_SENSITIVE) != 0);
      break;
    case UIA_HasKeyboardFocusPropertyId:
      v_bool(r, (in.flat & AWRA_A11Y_FOCUSED) != 0);
      break;
    case UIA_IsKeyboardFocusablePropertyId:
      v_bool(r, (in.flat & AWRA_A11Y_FOCUSABLE) != 0);
      break;
    case UIA_IsOffscreenPropertyId:
      v_bool(r, !(in.flat & AWRA_A11Y_SHOWING));
      break;
    case UIA_IsControlElementPropertyId:
    case UIA_IsContentElementPropertyId:
      v_bool(r, in.i.role != AWRA_ACCESSIBLE_ROLE_GENERIC || prov_hwnd(p));
      break;
    case UIA_IsPasswordPropertyId:
      v_bool(r, (in.flat & AWRA_A11Y_PASSWORD) != 0);
      break;
    case UIA_AcceleratorKeyPropertyId:
      if (in.i.shortcut) {
        v_str(r, in.i.shortcut);
      }
      break;
    case UIA_OrientationPropertyId:
      v_i4(r, (in.flat & AWRA_A11Y_HORIZONTAL) ? OrientationType_Horizontal :
              (in.flat & AWRA_A11Y_VERTICAL)   ? OrientationType_Vertical :
                                                 OrientationType_None);
      break;
    case UIA_HeadingLevelPropertyId:
      if (in.i.role == AWRA_ACCESSIBLE_ROLE_HEADING) {
        const int lv = in.i.level > 0 && in.i.level <= 9 ? in.i.level : 1;
        v_i4(r, 80050 + lv); /* HeadingLevel1 = 80051 */
      }
      break;
    case UIA_IsDialogPropertyId:
      v_bool(r, in.i.role == AWRA_ACCESSIBLE_ROLE_DIALOG || in.i.role == AWRA_ACCESSIBLE_ROLE_ALERT_DIALOG);
      break;
    case UIA_IsRequiredForFormPropertyId:
      v_bool(r, (in.flat & AWRA_A11Y_REQUIRED) != 0);
      break;
    case UIA_ItemStatusPropertyId:
      if (in.flat & AWRA_A11Y_BUSY) {
        v_str(r, "Busy");
      }
      break;
    case UIA_ProcessIdPropertyId:
      v_i4(r, (long)GetCurrentProcessId());
      break;
    case UIA_NativeWindowHandlePropertyId:
      if (prov_hwnd(p)) {
        v_i4(r, (long)(intptr_t)prov_hwnd(p));
      }
      break;
    default:
      break;
  }
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE simple_host(IRawElementProviderSimple *This,
                                             IRawElementProviderSimple **r)
{
  Prov *p = PROV_OF(This, simple);
  *r = NULL;
  HWND hwnd = prov_hwnd(p);
  if (hwnd && IsWindow(hwnd)) {
    return U.HostProviderFromHwnd(hwnd, r);
  }
  return S_OK;
}

static const IRawElementProviderSimpleVtbl g_simple_vtbl = {
    simple_qi, simple_addref, simple_release, simple_options, simple_pattern, simple_property,
    simple_host,
};

/* ----------------------------------------------------------------------- */
/* IRawElementProviderFragment */

static Prov *window_prov(AwraWidget *w)
{
  /* inside a popover with a window of its own: that one */
  for (AwraWidget *a = w; a; a = awra_a11y_parent(a)) {
    if (popup_hwnd(a)) {
      return prov_get(a, -1);
    }
  }
  AwraWidget *win = awra_window_of(w);
  return win ? prov_get(win, -1) : NULL;
}

/* children: presented widgets, then virtual items */
static int kids_of(const Prov *p, AwraWidget ***out, int *nitems)
{
  *out = NULL;
  *nitems = 0;
  if (p->item >= 0) {
    return 0;
  }
  int n = awra_a11y_children(p->w, NULL, 0);
  AwraWidget **v = n ? (AwraWidget **)malloc(sizeof(*v) * (size_t)n) : NULL;
  if (v) {
    awra_a11y_children(p->w, v, n);
    /* popovers in windows of their own are fragment roots of their own */
    int k = 0;
    for (int i = 0; i < n; i++) {
      if (!popup_hwnd(v[i])) {
        v[k++] = v[i];
      }
    }
    n = k;
  }
  *out = v;
  *nitems = awra_a11y_n_items(p->w);
  return v ? n : 0;
}

static Prov *child_prov(Prov *p, AwraWidget **kids, int n, int nitems, int i)
{
  if (i < 0 || i >= n + nitems) {
    return NULL;
  }
  return i < n ? prov_get(kids[i], -1) : prov_get(p->w, i - n);
}

static HRESULT STDMETHODCALLTYPE fragment_navigate(IRawElementProviderFragment *This,
                                                   enum NavigateDirection dir,
                                                   IRawElementProviderFragment **r)
{
  Prov *p = PROV_OF(This, fragment);
  *r = NULL;
  CHECK_ALIVE(p);
  Prov *found = NULL;
  if (dir == NavigateDirection_Parent) {
    if (prov_hwnd(p)) {
      return S_OK; /* the window's host is its parent */
    }
    if (p->item >= 0) {
      found = prov_get(p->w, -1);
    }
    else {
      AwraWidget *parent = awra_a11y_parent(p->w);
      found = parent ? prov_get(parent, -1) : NULL;
    }
  }
  else if (dir == NavigateDirection_FirstChild || dir == NavigateDirection_LastChild) {
    AwraWidget **kids;
    int nitems;
    const int n = kids_of(p, &kids, &nitems);
    found = child_prov(p, kids, n, nitems, dir == NavigateDirection_FirstChild ? 0 : n + nitems - 1);
    free(kids);
  }
  else if (!prov_hwnd(p)) {
    /* siblings: through the parent's children (popovers in windows of
     * their own left out) */
    Prov parent_view = {0};
    parent_view.w = p->item >= 0 ? p->w : awra_a11y_parent(p->w);
    parent_view.item = -1;
    if (parent_view.w) {
      AwraWidget **kids;
      int nitems;
      const int n = kids_of(&parent_view, &kids, &nitems);
      int index = -1;
      if (p->item >= 0) {
        index = n + p->item;
      }
      else {
        for (int i = 0; i < n && index < 0; i++) {
          index = kids[i] == p->w ? i : -1;
        }
      }
      if (index >= 0) {
        found = child_prov(&parent_view, kids, n, nitems,
                           dir == NavigateDirection_NextSibling ? index + 1 : index - 1);
      }
      free(kids);
    }
  }
  if (found) {
    *r = as_fragment(found);
  }
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE fragment_runtime_id(IRawElementProviderFragment *This,
                                                     SAFEARRAY **r)
{
  Prov *p = PROV_OF(This, fragment);
  *r = NULL;
  CHECK_ALIVE(p);
  if (prov_hwnd(p)) {
    return S_OK; /* the host provider gives the window's */
  }
  SAFEARRAY *a = U.SafeArrayCreateVector(VT_I4, 0, 3);
  if (!a) {
    return E_OUTOFMEMORY;
  }
  LONG idx;
  int v[3] = {UiaAppendRuntimeId, (int)p->id, p->item + 1};
  for (idx = 0; idx < 3; idx++) {
    U.SafeArrayPutElement(a, &idx, &v[idx]);
  }
  *r = a;
  return S_OK;
}

/* a provider's box on the screen, physical px */
static void screen_rect(Prov *p, const Info *in, struct UiaRect *r)
{
  const double s = awra_scale();
  POINT o = {0, 0};
  AwraWidget *win = awra_window_of(p->w);
  HWND hwnd = win ? (HWND)awra_window_native_handle(win) : NULL;
  if (hwnd) {
    ClientToScreen(hwnd, &o);
  }
  r->left = o.x + in->rect[0] * s;
  r->top = o.y + in->rect[1] * s;
  r->width = in->rect[2] * s;
  r->height = in->rect[3] * s;
}

static HRESULT STDMETHODCALLTYPE fragment_bounds(IRawElementProviderFragment *This,
                                                 struct UiaRect *r)
{
  Prov *p = PROV_OF(This, fragment);
  memset(r, 0, sizeof(*r));
  CHECK_ALIVE(p);
  if (prov_hwnd(p)) {
    return S_OK; /* the host's */
  }
  Info in;
  prov_info(p, &in);
  screen_rect(p, &in, r);
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE fragment_embedded(IRawElementProviderFragment *This,
                                                   SAFEARRAY **r)
{
  *r = NULL;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE fragment_set_focus(IRawElementProviderFragment *This)
{
  Prov *p = PROV_OF(This, fragment);
  CHECK_ALIVE(p);
  awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_FOCUS, 0);
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE fragment_root(IRawElementProviderFragment *This,
                                               IRawElementProviderFragmentRoot **r)
{
  Prov *p = PROV_OF(This, fragment);
  *r = NULL;
  CHECK_ALIVE(p);
  Prov *w = window_prov(p->w);
  if (w) {
    *r = (IRawElementProviderFragmentRoot *)&w->root;
  }
  return S_OK;
}

static const IRawElementProviderFragmentVtbl g_fragment_vtbl = {
    fragment_qi,        fragment_addref,   fragment_release,       fragment_navigate,
    fragment_runtime_id, fragment_bounds,  fragment_embedded,      fragment_set_focus,
    fragment_root,
};

/* ----------------------------------------------------------------------- */
/* IRawElementProviderFragmentRoot (windows) */

static HRESULT STDMETHODCALLTYPE root_from_point(IRawElementProviderFragmentRoot *This, double x,
                                                 double y, IRawElementProviderFragment **r)
{
  Prov *p = PROV_OF(This, root);
  *r = NULL;
  CHECK_ALIVE(p);
  /* the tree's coordinates are its window's, popovers' too */
  AwraWidget *win = awra_window_of(p->w);
  HWND whwnd = win ? (HWND)awra_window_native_handle(win) : NULL;
  POINT pt = {(LONG)x, (LONG)y};
  if (whwnd) {
    ScreenToClient(whwnd, &pt);
  }
  const double s = awra_scale();
  AwraWidget *hit = win ? awra_a11y_at_point(win, pt.x / s, pt.y / s) : NULL;
  if (!hit) {
    return S_OK;
  }
  /* only what belongs to this fragment root (a popup's window, or the
   * window without its popups' windows) */
  Prov *owner = window_prov(hit);
  const bool mine = owner == p;
  if (owner) {
    prov_release(owner);
  }
  if (!mine) {
    return S_OK;
  }
  /* a virtual item under the point */
  const int n = awra_a11y_n_items(hit);
  for (int i = 0; i < n; i++) {
    AwraA11yInfo ii;
    double ir[4];
    awra_a11y_item_info(hit, i, &ii, ir);
    if (pt.x / s >= ir[0] && pt.y / s >= ir[1] && pt.x / s < ir[0] + ir[2] && pt.y / s < ir[1] + ir[3]) {
      Prov *ip = prov_get(hit, i);
      *r = ip ? as_fragment(ip) : NULL;
      return S_OK;
    }
  }
  Prov *hp = prov_get(hit, -1);
  *r = hp ? as_fragment(hp) : NULL;
  return S_OK;
}

static Prov *focus_prov(AwraWidget *win)
{
  AwraWidget *f = awra_window_focus(win);
  if (!f) {
    return NULL;
  }
  const int n = awra_a11y_n_items(f);
  for (int i = 0; i < n; i++) {
    AwraA11yInfo ii;
    awra_a11y_item_info(f, i, &ii, NULL);
    if (ii.flags & AWRA_A11Y_FOCUSED) {
      return prov_get(f, i);
    }
  }
  /* a container's focused cell (a list's cursor) */
  for (int i = 0; i < f->nkids; i++) {
    if (awra_is_widget(f->kids[i])) {
      AwraA11yInfo ci;
      awra_a11y_info(f->kids[i], &ci);
      if (ci.flags & AWRA_A11Y_FOCUSED) {
        return prov_get(f->kids[i], -1);
      }
    }
  }
  return prov_get(f, -1);
}

static HRESULT STDMETHODCALLTYPE root_focus(IRawElementProviderFragmentRoot *This,
                                            IRawElementProviderFragment **r)
{
  Prov *p = PROV_OF(This, root);
  *r = NULL;
  CHECK_ALIVE(p);
  AwraWidget *win = awra_window_of(p->w);
  Prov *f = win ? focus_prov(win) : NULL;
  if (f) { /* the focus, if it is in this fragment root */
    Prov *owner = window_prov(f->w);
    if (owner != p) {
      prov_release(f);
      f = NULL;
    }
    if (owner) {
      prov_release(owner);
    }
  }
  *r = f ? as_fragment(f) : NULL;
  return S_OK;
}

static const IRawElementProviderFragmentRootVtbl g_root_vtbl = {
    root_qi, root_addref, root_release, root_from_point, root_focus,
};

/* ----------------------------------------------------------------------- */
/* Patterns */

static HRESULT STDMETHODCALLTYPE invoke_invoke(IInvokeProvider *This)
{
  Prov *p = PROV_OF(This, invoke);
  CHECK_ALIVE(p);
  if (!awra_sensitive(p->w)) {
    return (HRESULT)0x80131509L; /* UIA_E_ELEMENTNOTENABLED */
  }
  awra_widget_ref(p->w);
  awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_ACTIVATE, 0);
  if (listening()) {
    U.RaiseAutomationEvent(as_simple(p), UIA_Invoke_InvokedEventId);
  }
  awra_widget_unref(p->w);
  return S_OK;
}

static const IInvokeProviderVtbl g_invoke_vtbl = {invoke_qi, invoke_addref, invoke_release,
                                                  invoke_invoke};

static enum ToggleState toggle_state_of(const Info *in)
{
  if (in->flat & AWRA_A11Y_MIXED) {
    return ToggleState_Indeterminate;
  }
  return (in->flat & (AWRA_A11Y_CHECKED | AWRA_A11Y_PRESSED)) ? ToggleState_On : ToggleState_Off;
}

static HRESULT STDMETHODCALLTYPE toggle_toggle(IToggleProvider *This)
{
  Prov *p = PROV_OF(This, toggle);
  CHECK_ALIVE(p);
  awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_ACTIVATE, 0);
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE toggle_state(IToggleProvider *This, enum ToggleState *r)
{
  Prov *p = PROV_OF(This, toggle);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  *r = toggle_state_of(&in);
  return S_OK;
}

static const IToggleProviderVtbl g_toggle_vtbl = {toggle_qi, toggle_addref, toggle_release,
                                                  toggle_toggle, toggle_state};

static HRESULT STDMETHODCALLTYPE value_set(IValueProvider *This, LPCWSTR v)
{
  Prov *p = PROV_OF(This, value);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  if (!has_text_value(&in) || (in.flat & AWRA_A11Y_READ_ONLY) || !awra_sensitive(p->w)) {
    return (HRESULT)0x80131509L;
  }
  char *s = utf8_wide(v);
  if (!s) {
    return E_OUTOFMEMORY;
  }
  awra_editable_delete(p->w, 0, awra_editable_length(p->w));
  awra_editable_insert(p->w, 0, s);
  free(s);
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE value_get(IValueProvider *This, BSTR *r)
{
  Prov *p = PROV_OF(This, value);
  *r = NULL;
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  if (in.i.role == AWRA_ACCESSIBLE_ROLE_COMBO_BOX) {
    /* the shown choice: the combo's content */
    char *n = NULL;
    AwraWidget *kids[4];
    const int k = awra_a11y_children(p->w, kids, 4);
    for (int i = 0; i < k && i < 4 && !n; i++) {
      n = awra_a11y_name(kids[i]);
      if (n && !n[0]) {
        free(n);
        n = NULL;
      }
    }
    *r = bstr_utf8(n ? n : "");
    free(n);
    return S_OK;
  }
  *r = bstr_utf8((in.flat & AWRA_A11Y_PASSWORD) ? "" : in.i.text);
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE value_read_only(IValueProvider *This, WINBOOL *r)
{
  Prov *p = PROV_OF(This, value);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  *r = !has_text_value(&in) || (in.flat & AWRA_A11Y_READ_ONLY) || !(in.flat & AWRA_A11Y_SENSITIVE);
  return S_OK;
}

static const IValueProviderVtbl g_value_vtbl = {value_qi, value_addref, value_release, value_set,
                                                value_get, value_read_only};

static HRESULT STDMETHODCALLTYPE range_set(IRangeValueProvider *This, double v)
{
  Prov *p = PROV_OF(This, range);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  if (v < in.i.min || v > in.i.max) {
    return E_INVALIDARG;
  }
  return awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_SET_VALUE, v) ? S_OK : (HRESULT)0x80131509L;
}

#define RANGE_GETTER(name, expr)                                                         \
  static HRESULT STDMETHODCALLTYPE range_##name(IRangeValueProvider *This, double *r)   \
  {                                                                                      \
    Prov *p = PROV_OF(This, range);                                                      \
    CHECK_ALIVE(p);                                                                      \
    Info in;                                                                             \
    prov_info(p, &in);                                                                   \
    *r = (expr);                                                                         \
    return S_OK;                                                                         \
  }
RANGE_GETTER(value, in.i.value)
RANGE_GETTER(maximum, in.i.max)
RANGE_GETTER(minimum, in.i.min)
RANGE_GETTER(large, in.i.step > 0 ? in.i.step * 10 : (in.i.max - in.i.min) / 10)
RANGE_GETTER(small, in.i.step > 0 ? in.i.step : (in.i.max - in.i.min) / 100)

static HRESULT STDMETHODCALLTYPE range_read_only(IRangeValueProvider *This, WINBOOL *r)
{
  Prov *p = PROV_OF(This, range);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  *r = in.i.role == AWRA_ACCESSIBLE_ROLE_PROGRESS_BAR || in.i.role == AWRA_ACCESSIBLE_ROLE_METER ||
       !(in.flat & AWRA_A11Y_SENSITIVE);
  return S_OK;
}

static const IRangeValueProviderVtbl g_range_vtbl = {
    range_qi,      range_addref,  range_release, range_set,   range_value,
    range_read_only, range_maximum, range_minimum, range_large, range_small,
};

static HRESULT STDMETHODCALLTYPE selitem_select(ISelectionItemProvider *This)
{
  Prov *p = PROV_OF(This, selitem);
  CHECK_ALIVE(p);
  if (!awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_SELECT, 0)) {
    awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_ACTIVATE, 0);
  }
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE selitem_add(ISelectionItemProvider *This)
{
  Prov *p = PROV_OF(This, selitem);
  CHECK_ALIVE(p);
  awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_SELECT, 0);
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE selitem_remove(ISelectionItemProvider *This)
{
  Prov *p = PROV_OF(This, selitem);
  CHECK_ALIVE(p);
  awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_DESELECT, 0);
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE selitem_is(ISelectionItemProvider *This, WINBOOL *r)
{
  Prov *p = PROV_OF(This, selitem);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  *r = (in.flat & AWRA_A11Y_SELECTED) != 0;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE selitem_container(ISelectionItemProvider *This,
                                                   IRawElementProviderSimple **r)
{
  Prov *p = PROV_OF(This, selitem);
  *r = NULL;
  CHECK_ALIVE(p);
  AwraWidget *c = p->item >= 0 ? p->w : awra_a11y_parent(p->w);
  Prov *cp = c ? prov_get(c, -1) : NULL;
  *r = cp ? as_simple(cp) : NULL;
  return S_OK;
}

static const ISelectionItemProviderVtbl g_selitem_vtbl = {
    selitem_qi,  selitem_addref, selitem_release,  selitem_select,
    selitem_add, selitem_remove, selitem_is,       selitem_container,
};

static HRESULT STDMETHODCALLTYPE selection_get(ISelectionProvider *This, SAFEARRAY **r)
{
  Prov *p = PROV_OF(This, selection);
  *r = NULL;
  CHECK_ALIVE(p);
  AwraWidget **kids;
  int nitems;
  const int n = kids_of(p, &kids, &nitems);
  Prov *sel[256];
  int count = 0;
  for (int i = 0; i < n + nitems && count < 256; i++) {
    AwraA11yInfo info;
    if (i < n) {
      awra_a11y_info(kids[i], &info);
    }
    else {
      awra_a11y_item_info(p->w, i - n, &info, NULL);
    }
    const bool on = (info.defined & (1u << AWRA_ACCESSIBLE_STATE_SELECTED)) ?
                        info.state[AWRA_ACCESSIBLE_STATE_SELECTED] != 0 :
                        (info.defined & (1u << AWRA_ACCESSIBLE_STATE_CHECKED)) &&
                            info.state[AWRA_ACCESSIBLE_STATE_CHECKED] == AWRA_ACCESSIBLE_TRISTATE_TRUE;
    if (on) {
      Prov *c = child_prov(p, kids, n, nitems, i);
      if (c) {
        sel[count++] = c;
      }
    }
  }
  free(kids);
  SAFEARRAY *a = U.SafeArrayCreateVector(VT_UNKNOWN, 0, (ULONG)count);
  for (LONG i = 0; i < count; i++) {
    if (a) {
      U.SafeArrayPutElement(a, &i, (IUnknown *)as_simple(sel[i])); /* adds its reference */
    }
    prov_release(sel[i]);
  }
  *r = a;
  return a ? S_OK : E_OUTOFMEMORY;
}

static HRESULT STDMETHODCALLTYPE selection_multiple(ISelectionProvider *This, WINBOOL *r)
{
  Prov *p = PROV_OF(This, selection);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  *r = (in.flat & AWRA_A11Y_MULTISELECTABLE) != 0;
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE selection_required(ISelectionProvider *This, WINBOOL *r)
{
  *r = FALSE;
  return S_OK;
}

static const ISelectionProviderVtbl g_selection_vtbl = {
    selection_qi, selection_addref, selection_release, selection_get, selection_multiple,
    selection_required,
};

static HRESULT STDMETHODCALLTYPE expand_set(Prov *p, bool expand)
{
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  if (((in.flat & AWRA_A11Y_EXPANDED) != 0) != expand) {
    if (!awra_a11y_do(p->w, p->item, expand ? AWRA_A11Y_DO_EXPAND : AWRA_A11Y_DO_COLLAPSE, 0)) {
      awra_a11y_do(p->w, p->item, AWRA_A11Y_DO_ACTIVATE, 0);
    }
  }
  return S_OK;
}

static HRESULT STDMETHODCALLTYPE expand_expand(IExpandCollapseProvider *This)
{
  return expand_set(PROV_OF(This, expand), true);
}

static HRESULT STDMETHODCALLTYPE expand_collapse(IExpandCollapseProvider *This)
{
  return expand_set(PROV_OF(This, expand), false);
}

static HRESULT STDMETHODCALLTYPE expand_state(IExpandCollapseProvider *This,
                                              enum ExpandCollapseState *r)
{
  Prov *p = PROV_OF(This, expand);
  CHECK_ALIVE(p);
  Info in;
  prov_info(p, &in);
  *r = (in.flat & AWRA_A11Y_EXPANDED) ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed;
  return S_OK;
}

static const IExpandCollapseProviderVtbl g_expand_vtbl = {
    expand_qi, expand_addref, expand_release, expand_expand, expand_collapse, expand_state,
};

/* ======================================================================= */
/* Events */

static Prov *event_prov(AwraWidget *w, int item)
{
  if (!listening() || !w) {
    return NULL;
  }
  return prov_get(w, item);
}

static void raise_prop(AwraWidget *w, int item, PROPERTYID id, VARIANT now)
{
  Prov *p = event_prov(w, item);
  if (p) {
    VARIANT old;
    v_init(&old);
    U.RaisePropertyChanged(as_simple(p), id, old, now);
    prov_release(p);
  }
}

static void ev_state(AwraWidget *w, int item, unsigned flag, bool on)
{
  if (!listening()) {
    return;
  }
  VARIANT v;
  v_init(&v);
  switch (flag) {
    case AWRA_A11Y_FOCUSED:
      if (on) {
        Prov *p = event_prov(w, item);
        if (p) {
          U.RaiseAutomationEvent(as_simple(p), UIA_AutomationFocusChangedEventId);
          prov_release(p);
        }
      }
      v_bool(&v, on);
      raise_prop(w, item, UIA_HasKeyboardFocusPropertyId, v);
      break;
    case AWRA_A11Y_CHECKED:
    case AWRA_A11Y_PRESSED:
    case AWRA_A11Y_MIXED:
      v_i4(&v, on ? (flag == AWRA_A11Y_MIXED ? ToggleState_Indeterminate : ToggleState_On) :
                    ToggleState_Off);
      raise_prop(w, item, UIA_ToggleToggleStatePropertyId, v);
      break;
    case AWRA_A11Y_EXPANDED:
      v_i4(&v, on ? ExpandCollapseState_Expanded : ExpandCollapseState_Collapsed);
      raise_prop(w, item, UIA_ExpandCollapseExpandCollapseStatePropertyId, v);
      break;
    case AWRA_A11Y_SELECTED: {
      v_bool(&v, on);
      raise_prop(w, item, UIA_SelectionItemIsSelectedPropertyId, v);
      if (on) {
        Prov *p = event_prov(w, item);
        if (p) {
          U.RaiseAutomationEvent(as_simple(p), UIA_SelectionItem_ElementSelectedEventId);
          prov_release(p);
        }
      }
      break;
    }
    case AWRA_A11Y_SENSITIVE:
      v_bool(&v, on);
      raise_prop(w, item, UIA_IsEnabledPropertyId, v);
      break;
    case AWRA_A11Y_SHOWING:
      v_bool(&v, !on);
      raise_prop(w, item, UIA_IsOffscreenPropertyId, v);
      break;
    default:
      break;
  }
}

static void ev_name(AwraWidget *w)
{
  if (!listening()) {
    return;
  }
  const AwraA11ySnap *s = awra_a11y_snap(w);
  VARIANT v;
  v_str(&v, s ? s->name : "");
  raise_prop(w, -1, UIA_NamePropertyId, v);
  U.SysFreeString(v.bstrVal);
}

static void ev_description(AwraWidget *w)
{
  if (!listening()) {
    return;
  }
  const AwraA11ySnap *s = awra_a11y_snap(w);
  VARIANT v;
  v_str(&v, s ? s->description : "");
  raise_prop(w, -1, UIA_HelpTextPropertyId, v);
  U.SysFreeString(v.bstrVal);
}

static void ev_value(AwraWidget *w)
{
  if (!listening()) {
    return;
  }
  AwraA11yInfo info;
  awra_a11y_info(w, &info);
  VARIANT v;
  v_init(&v);
  v.vt = VT_R8;
  v.dblVal = info.value;
  raise_prop(w, -1, UIA_RangeValueValuePropertyId, v);
}

static void ev_text(AwraWidget *w, bool insert, int offset, int length, const char *text, int bytes)
{
  if (!listening()) {
    return;
  }
  const AwraA11ySnap *s = awra_a11y_snap(w);
  VARIANT v;
  v_str(&v, s && s->text ? s->text : "");
  raise_prop(w, -1, UIA_ValueValuePropertyId, v);
  U.SysFreeString(v.bstrVal);
}

static void structure(AwraWidget *parent)
{
  Prov *p = event_prov(parent, -1);
  if (p && U.RaiseStructureChanged) {
    int rid[3] = {UiaAppendRuntimeId, (int)p->id, 0};
    U.RaiseStructureChanged(as_simple(p), StructureChangeType_ChildrenInvalidated, rid, 3);
  }
  if (p) {
    prov_release(p);
  }
}

static void ev_children(AwraWidget *parent, AwraWidget *child, int index, bool added)
{
  if (listening()) {
    structure(parent);
  }
}

static void ev_items(AwraWidget *w, int old_count, int new_count)
{
  if (listening()) {
    structure(w);
  }
}

static void ev_selection(AwraWidget *container)
{
  Prov *p = event_prov(container, -1);
  if (p) {
    U.RaiseAutomationEvent(as_simple(p), UIA_Selection_InvalidatedEventId);
    prov_release(p);
  }
}

static void ev_window(AwraWidget *win, int what)
{
  if (!listening()) {
    return;
  }
  if (what == AWRA_A11Y_WINDOW_CREATE || what == AWRA_A11Y_WINDOW_DESTROY) {
    Prov *p = event_prov(win, -1);
    if (p) {
      U.RaiseAutomationEvent(as_simple(p), what == AWRA_A11Y_WINDOW_CREATE ?
                                               UIA_Window_WindowOpenedEventId :
                                               UIA_Window_WindowClosedEventId);
      prov_release(p);
    }
  }
}

static void ev_announce(AwraWidget *w, const char *message, int priority)
{
  if (!listening() || !U.RaiseNotification) {
    return;
  }
  AwraWidget *src = w;
  while (src && !awra_a11y_presented(src)) {
    src = src->parent;
  }
  Prov *p = event_prov(src ? src : w, -1);
  if (!p) {
    return;
  }
  BSTR text = bstr_utf8(message), activity = U.SysAllocString(L"awra-announcement");
  U.RaiseNotification(as_simple(p), NotificationKind_Other,
                      priority >= AWRA_ACCESSIBLE_ANNOUNCEMENT_PRIORITY_HIGH ?
                          NotificationProcessing_ImportantMostRecent :
                          NotificationProcessing_MostRecent,
                      text, activity);
  U.SysFreeString(text);
  U.SysFreeString(activity);
  prov_release(p);
}

static void ev_freed(AwraWidget *w, AwraA11ySnap *snap)
{
  Prov *p = (Prov *)snap->platform;
  if (!p) {
    return;
  }
  snap->platform = NULL;
  p->w = NULL; /* clients holding it get "not available" */
  if (U.DisconnectProvider) {
    U.DisconnectProvider(as_simple(p));
  }
  prov_release(p);
}

static const AwraA11yBackend g_backend = {
    .state = ev_state,
    .name = ev_name,
    .description = ev_description,
    .value = ev_value,
    .text = ev_text,
    .selection = ev_selection,
    .children = ev_children,
    .items = ev_items,
    .window = ev_window,
    .announce = ev_announce,
    .freed = ev_freed,
};

/* ======================================================================= */
/* WM_GETOBJECT */

static intptr_t getobject(void *hwnd_, unsigned msg, uintptr_t wp, intptr_t lp, BKContext *ctx,
                          int *r_handled)
{
  HWND hwnd = (HWND)hwnd_;
  AwraWidget *win = ctx ? (AwraWidget *)ctx->user : NULL;
  if (msg == WM_DESTROY) {
    /* let go of the window's providers */
    if (U.ok) {
      U.ReturnRawElementProvider(hwnd, 0, 0, NULL);
    }
    return 0;
  }
  if ((long)lp != UiaRootObjectId || !win || !awra_is_widget(win)) {
    return 0;
  }
  if (!uia_load()) {
    return 0;
  }
  if (!awra_a11y_on) {
    awra_a11y_set_backend(&g_backend);
  }
  /* a popover's own window: it is that fragment root */
  AwraWidget *popup = NULL;
  for (int i = 0; ctx && i < ctx->npopups && !popup; i++) {
    if (awra_is_widget(ctx->popups[i]) && popup_hwnd(ctx->popups[i]) == hwnd) {
      popup = ctx->popups[i];
    }
  }
  Prov *p = prov_get(popup ? popup : win, -1);
  if (!p) {
    return 0;
  }
  if (!popup) {
    p->hwnd = hwnd;
  }
  const LRESULT r = U.ReturnRawElementProvider(hwnd, (WPARAM)wp, (LPARAM)lp, as_simple(p));
  prov_release(p);
  *r_handled = 1;
  return (intptr_t)r;
}

void awra_a11y_platform_start(void)
{
  static bool started;
  if (started) {
    return;
  }
  started = true;
  const char *v = getenv("AWRA_A11Y");
  if (v && !strcmp(v, "0")) {
    return;
  }
  BK_window_set_getobject_hook(getobject);
}

void awra_a11y_platform_stop(void)
{
  BK_window_set_getobject_hook(NULL);
}

#endif /* _WIN32 */
