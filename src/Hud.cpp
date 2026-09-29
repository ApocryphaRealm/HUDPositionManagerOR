#include "Hud.h"

#include "Settings.h"
#include "Ue.h"

namespace hud
{
	namespace
	{
		constexpr const wchar_t* kLayoutClass = L"/Game/UI/WBP_PrimaryGameLayout.WBP_PrimaryGameLayout_C";   // the root of all the game's UI

		// ESlateVisibility
		constexpr std::uint8_t kVisible = 0, kCollapsed = 1, kHidden = 2, kSelfHitTestInvisible = 4;

		struct Tracked
		{
			ue::Handle widget;
			std::string name;               // instance and class, for the page and the log
			bool        haveBase = false;
			double      baseX = 0, baseY = 0, baseSX = 1, baseSY = 1;   // the game's own transform
			double      lastX = 0, lastY = 0, lastSX = 1, lastSY = 1;   // what this mod wrote last
			bool        wrote = false;
			double      pivotX = 0.5, pivotY = 0.5;   // the element's own pivot, put back when it returns to the game's layout
			bool        pivotSet = false;
			bool        hiddenByUs = false;
			std::uint8_t visibleBefore = kSelfHitTestInvisible;
			bool        forced = false;     // "Always visible" is holding it up
			bool        measured = false;   // the rectangle below is fresh enough to bound the sliders
			double      vx = 0, vy = 0, vw = 0, vh = 0;
			double      baseVX = 0, baseVY = 0;   // vx / vy minus the offset that was applied when measured: the bound's anchor
			ULONGLONG   measuredAt = 0;
		};
		double g_viewW = 0, g_viewH = 0;

		std::vector<Tracked> g_el(elements::Count());
		ue::Handle           g_layout;
		ULONGLONG            g_lastFind = 0, g_lastLayoutScan = 0;

		std::mutex                 g_lock;   // g_status and g_hudFound, for the page and the tool
		std::vector<ElementStatus> g_status(elements::Count());
		bool                       g_hudFound = false;

		// ---- reflection, cached per (class, name) ----

		std::int32_t Off(UE::UStruct* a_struct, const char* a_name)
		{
			static std::unordered_map<std::string, std::int32_t> cache;
			const std::string key = std::format("{}:{}", static_cast<const void*>(a_struct), a_name);
			if (const auto it = cache.find(key); it != cache.end()) {
				return it->second;
			}
			const auto o = a_struct ? ue::Offset(a_struct, a_name) : -1;
			cache.emplace(key, o);
			return o;
		}

		UE::UObject* ObjProp(UE::UObject* a_obj, const char* a_name)
		{
			auto** p = a_obj ? ue::At<UE::UObject*>(a_obj, Off(a_obj->GetClass(), a_name)) : nullptr;
			return p ? *p : nullptr;
		}

		struct RawArray
		{
			UE::UObject** data;
			std::int32_t  num;
			std::int32_t  max;
		};

		UE::UClass* UserWidgetClass() { static auto* c = ue::Class(L"/Script/UMG.UserWidget"); return c; }
		UE::UClass* PanelClass() { static auto* c = ue::Class(L"/Script/UMG.PanelWidget"); return c; }
		UE::UClass* WidgetClass() { static auto* c = ue::Class(L"/Script/UMG.Widget"); return c; }

		std::wstring Wide(const std::string& a_s) { return std::wstring(a_s.begin(), a_s.end()); }

		// ---- finding the elements ----

		struct Walk
		{
			std::vector<UE::UObject*> byClass = std::vector<UE::UObject*>(elements::Count(), nullptr);
			std::vector<UE::UObject*> byName = std::vector<UE::UObject*>(elements::Count(), nullptr);
			int                       count = 0;
		};

		void Visit(UE::UObject* a_widget, int a_depth, Walk& a_walk)
		{
			if (!a_widget || a_depth > 40 || ++a_walk.count > 5000) {
				return;
			}
			auto*             cls = a_widget->GetClass();
			const std::string clsName = ue::NameOf(cls);
			const std::string name = ue::NameOf(a_widget);
			const auto&       all = elements::All();
			for (std::size_t i = 0; i < all.size(); ++i) {
				if (!a_walk.byClass[i]) {
					for (const auto* c : all[i].classes) {
						if (Wide(clsName) == c) {
							a_walk.byClass[i] = a_widget;
						}
					}
				}
				if (!a_walk.byName[i]) {
					for (const auto* n : all[i].names) {
						if (name == n) {
							a_walk.byName[i] = a_widget;
						}
					}
				}
			}
			if (UserWidgetClass() && cls->IsChildOf(UserWidgetClass())) {
				Visit(ObjProp(ObjProp(a_widget, "WidgetTree"), "RootWidget"), a_depth + 1, a_walk);
			}
			if (PanelClass() && cls->IsChildOf(PanelClass())) {
				auto* slots = ue::At<RawArray>(a_widget, Off(cls, "Slots"));
				for (std::int32_t i = 0; slots && slots->data && i < slots->num && i < 2048; ++i) {
					Visit(ObjProp(slots->data[i], "Content"), a_depth + 1, a_walk);
				}
			}
			// The layout's layers are CommonUI activatable-widget STACKS (UVActivatableWidgetStack), not panels: the HUD
			// is the game layer's DisplayedWidget, and the stack keeps every pushed widget in WidgetList (the first
			// walk stopped at 21 widgets and found no element, 2026-09-29 11:21).
			if (auto* displayed = ObjProp(a_widget, "DisplayedWidget")) {
				Visit(displayed, a_depth + 1, a_walk);
			}
			if (const auto off = Off(cls, "WidgetList"); off >= 0) {
				if (auto* list = ue::At<RawArray>(a_widget, off)) {
					for (std::int32_t i = 0; list->data && i < list->num && i < 256; ++i) {
						Visit(list->data[i], a_depth + 1, a_walk);
					}
				}
			}
		}

		UE::UObject* Layout()
		{
			if (auto* l = g_layout.Get()) {
				return l;
			}
			const ULONGLONG now = GetTickCount64();
			if (now - g_lastLayoutScan < 2000) {
				return nullptr;   // the object array scan is not cheap: at most every 2 s (rule 17)
			}
			g_lastLayoutScan = now;
			auto* found = ue::FirstOf(ue::Class(kLayoutClass));
			g_layout.Set(found);
			if (found) {
				logger::info("hud: the game's layout found ({})", ue::NameOf(found));
			}
			return found;
		}

		void Find(UE::UObject* a_layout)
		{
			Walk w;
			Visit(a_layout, 0, w);
			const auto& all = elements::All();
			int         found = 0;
			std::string missing;
			for (std::size_t i = 0; i < all.size(); ++i) {
				auto* widget = w.byClass[i] ? w.byClass[i] : w.byName[i];
				auto& t = g_el[i];
				if (widget == t.widget.Get() && widget) {
					++found;
					continue;
				}
				if (!widget) {
					missing += std::string(missing.empty() ? "" : ", ") + all[i].key;
					continue;
				}
				t = Tracked{};
				t.widget.Set(widget);
				t.name = std::format("{} ({})", ue::NameOf(widget), ue::NameOf(widget->GetClass()));
				logger::info("hud: {} is {}{}", all[i].key, t.name, w.byClass[i] ? "" : " - matched by its name, not its class");
				++found;
			}
			static std::string lastMissing = "?";
			if (missing != lastMissing) {
				lastMissing = missing;
				logger::info("hud: {} of {} elements found in {} widgets{}", found, all.size(), w.count,
					missing.empty() ? "" : " - not in this HUD (yet): " + missing);
			}
		}

		// ---- the widget's transform, visibility and opacity (read from its reflected properties) ----

		struct Transform
		{
			double x = 0, y = 0, sx = 1, sy = 1;
		};

		bool ReadTransform(UE::UObject* a_w, Transform& a_t)
		{
			static auto* st = UE::StaticFindObject<UE::UStruct>(nullptr, nullptr, L"/Script/UMG.WidgetTransform");
			const auto   off = Off(WidgetClass(), "RenderTransform");
			const auto   tr = Off(st, "Translation");
			const auto   sc = Off(st, "Scale");
			if (off < 0 || tr < 0 || sc < 0) {
				return false;
			}
			const auto* base = reinterpret_cast<const std::uint8_t*>(a_w) + off;
			const auto* t = reinterpret_cast<const double*>(base + tr);   // FVector2D: two doubles
			const auto* s = reinterpret_cast<const double*>(base + sc);
			a_t = { t[0], t[1], s[0], s[1] };
			return true;
		}

		std::uint8_t Visibility(UE::UObject* a_w)
		{
			const auto* v = ue::At<std::uint8_t>(a_w, Off(WidgetClass(), "Visibility"));
			return v ? *v : kVisible;
		}

		float Opacity(UE::UObject* a_w)
		{
			const auto* o = ue::At<float>(a_w, Off(WidgetClass(), "RenderOpacity"));
			return o ? *o : 1.0f;
		}

		void Call2(UE::UObject* a_w, const wchar_t* a_fn, const char* a_param, double a_x, double a_y)
		{
			ue::Call c(a_w, a_fn);
			const double v[2] = { a_x, a_y };
			if (void* p = c.At(a_param)) {
				std::memcpy(p, v, sizeof(v));
				c.Run();
			}
		}

		void SetVisibility(UE::UObject* a_w, std::uint8_t a_v)
		{
			ue::Call c(a_w, L"SetVisibility");
			c.Set("InVisibility", a_v);
			c.Run();
		}

		void SetOpacity(UE::UObject* a_w, float a_o)
		{
			ue::Call c(a_w, L"SetRenderOpacity");
			c.Set("InOpacity", a_o);
			c.Run();
		}

		// "Always visible" for a bar (2026-09-29 probe): WBP_ModernHud_Health holds a WBP_ModernHud_StatusBar child whose
		// FadeOut animation drives its ProgressBar image's RenderOpacity to 0 - the element's root never changes. So the
		// element's subtree is walked (a bar is three widgets): every user widget carrying a FadeOut animation has it
		// stopped while it plays, and every widget under it whose opacity fell is put back to 1. Returns how many
		// widgets were held up this frame.
		int HoldSubtreeVisible(UE::UObject* a_widget, int a_depth, bool& a_stoppedFade)
		{
			if (!a_widget || a_depth > 8) {
				return 0;
			}
			int   held = 0;
			auto* cls = a_widget->GetClass();
			if (UserWidgetClass() && cls->IsChildOf(UserWidgetClass())) {
				if (auto* fade = ObjProp(a_widget, "FadeOut")) {
					ue::Call playing(a_widget, L"IsAnimationPlaying");
					playing.Set("InAnimation", fade);
					const bool* isPlaying = playing.Run() ? static_cast<const bool*>(playing.At("ReturnValue")) : nullptr;
					if (isPlaying && *isPlaying) {
						ue::Call stop(a_widget, L"StopAnimation");
						stop.Set("InAnimation", fade);
						stop.Run();
						a_stoppedFade = true;
					}
				}
				held += HoldSubtreeVisible(ObjProp(ObjProp(a_widget, "WidgetTree"), "RootWidget"), a_depth + 1, a_stoppedFade);
			}
			if (PanelClass() && cls->IsChildOf(PanelClass())) {
				auto* slots = ue::At<RawArray>(a_widget, Off(cls, "Slots"));
				for (std::int32_t i = 0; slots && slots->data && i < slots->num && i < 64; ++i) {
					held += HoldSubtreeVisible(ObjProp(slots->data[i], "Content"), a_depth + 1, a_stoppedFade);
				}
			}
			if (a_depth > 0 && Opacity(a_widget) < 0.999f) {
				SetOpacity(a_widget, 1.0f);
				++held;
			}
			return held;
		}

		// ---- the element's rectangle on screen (viewport pixels) ----
		// UWidget::GetCachedGeometry (the geometry it was last drawn with, its render transform included), then the
		// engine's own SlateBlueprintLibrary::LocalToViewport of its (0,0) and GetLocalSize; the viewport's size from
		// WidgetLayoutLibrary::GetViewportSize. All through ProcessEvent, at most twice a second per element.
		UE::UObject* SlateLib() { static auto* o = UE::StaticFindObject<UE::UObject>(nullptr, nullptr, L"/Script/UMG.Default__SlateBlueprintLibrary"); return o; }
		UE::UObject* LayoutLib() { static auto* o = UE::StaticFindObject<UE::UObject>(nullptr, nullptr, L"/Script/UMG.Default__WidgetLayoutLibrary"); return o; }

		bool Measure(UE::UObject* a_w, Tracked& a_t)
		{
			ue::Call geom(a_w, L"GetCachedGeometry");
			if (!geom || !SlateLib() || !LayoutLib()) {
				return false;
			}
			geom.Run();
			const void* g = geom.At("ReturnValue");
			const auto  gsize = static_cast<std::size_t>(reinterpret_cast<UE::UStruct*>(geom.Function())->propertiesSize);   // the FGeometry alone
			if (!g || gsize == 0 || gsize > 128) {
				return false;
			}
			ue::Call toView(SlateLib(), L"LocalToViewport");
			ue::Call size(SlateLib(), L"GetLocalSize");
			ue::Call view(LayoutLib(), L"GetViewportSize");
			ue::Call dpi(LayoutLib(), L"GetViewportScale");   // pixels per viewport unit: the size below comes in pixels
			if (!toView || !size || !view || !dpi || !toView.At("Geometry") || !size.At("Geometry")) {
				return false;
			}
			std::memcpy(toView.At("Geometry"), g, gsize);
			std::memcpy(size.At("Geometry"), g, gsize);
			toView.Set("WorldContextObject", a_w);
			const double zero[2] = { 0.0, 0.0 };
			if (void* lc = toView.At("LocalCoordinate")) {
				std::memcpy(lc, zero, sizeof(zero));
			}
			view.Set("WorldContextObject", a_w);
			dpi.Set("WorldContextObject", a_w);
			if (!toView.Run() || !size.Run() || !view.Run() || !dpi.Run()) {
				return false;
			}
			const auto* scalePtr = static_cast<const float*>(dpi.At("ReturnValue"));
			const double dpiScale = scalePtr && *scalePtr > 0.0f ? *scalePtr : 1.0;
			const auto* vp = static_cast<const double*>(toView.At("ViewportPosition"));
			const auto* sz = static_cast<const double*>(size.At("ReturnValue"));
			const auto* vs = static_cast<const double*>(view.At("ReturnValue"));
			if (!vp || !sz || !vs || vs[0] <= 0.0 || vs[1] <= 0.0) {
				return false;
			}
			a_t.vx = vp[0];
			a_t.vy = vp[1];
			a_t.vw = sz[0] * a_t.lastSX;   // GetLocalSize is the layout size; the render scale is this mod's own
			a_t.vh = sz[1] * a_t.lastSY;
			// ViewportPosition, GetLocalSize and RenderTranslation share the layout's units (1920x1080 at every DPI, the
			// health bar's centre measured at exactly 960 on a 3200x1800 display); GetViewportSize is in pixels
			g_viewW = vs[0] / dpiScale;
			g_viewH = vs[1] / dpiScale;
			a_t.baseVX = a_t.vx - a_t.lastX;   // the geometry was painted with the offset written last frame
			a_t.baseVY = a_t.vy - a_t.lastY;
			a_t.measured = a_t.vw > 0.0 && a_t.vh > 0.0;
			return a_t.measured;
		}

		// the offset an element gets: its own, plus the offset of what it moves with (chains followed, loops cut)
		std::pair<double, double> Offset(const settings::Values& a_s, std::size_t a_i, int a_depth = 0)
		{
			const auto& all = elements::All();
			const auto& e = a_s.elements[a_i];
			double      x = e.x, y = e.y;
			std::string with = e.moveWith;
			if (with.empty() && a_s.linkBars && all[a_i].barLink) {
				with = all[a_i].barLink;
			}
			if (!with.empty() && a_depth < 8) {
				if (const int j = elements::IndexOf(with); j >= 0 && static_cast<std::size_t>(j) != a_i) {
					const auto [wx, wy] = Offset(a_s, static_cast<std::size_t>(j), a_depth + 1);
					x += wx;
					y += wy;
				}
			}
			return { x, y };
		}

		void Apply(const settings::Values& a_s, bool a_gameplay)
		{
			const auto& all = elements::All();
			for (std::size_t i = 0; i < all.size(); ++i) {
				auto&         t = g_el[i];
				auto*         w = t.widget.Get();
				ElementStatus st;
				if (!w) {
					std::scoped_lock l(g_lock);
					g_status[i] = st;
					continue;
				}
				const auto& e = a_s.elements[i];
				Transform   now;
				if (!ReadTransform(w, now)) {
					continue;
				}
				// the base: what the game has; a value this mod did not write is the game's own, and the offset rides on it
				const double eps = 0.01;
				if (!t.haveBase || !t.wrote || std::abs(now.x - t.lastX) > eps || std::abs(now.y - t.lastY) > eps ||
					std::abs(now.sx - t.lastSX) > 1e-4 || std::abs(now.sy - t.lastSY) > 1e-4) {
					if (t.haveBase && t.wrote) {
						logger::debug("hud: {} was moved by the game to ({:.1f}, {:.1f}) x{:.2f} - the layout follows it", all[i].key, now.x, now.y, now.sx);
					}
					t.baseX = now.x;
					t.baseY = now.y;
					t.baseSX = now.sx;
					t.baseSY = now.sy;
					t.haveBase = true;
					t.wrote = false;
				}
				auto [ox, oy] = a_s.enabled ? Offset(a_s, i) : std::pair<double, double>{ 0.0, 0.0 };
				// the settings hold percent of the screen; the render transform takes layout units (1920 x 1080 at every
				// DPI, wider on a wide screen - the measured viewport, or the designer's size until it is measured)
				const double unitW = g_viewW > 0.0 ? g_viewW : 1920.0, unitH = g_viewH > 0.0 ? g_viewH : 1080.0;
				ox = ox / 100.0 * unitW;
				oy = oy / 100.0 * unitH;
				const double scale = a_s.enabled ? e.scale : 1.0;
				// the rectangle on screen, twice a second; the offset is clamped so the element never leaves the screen
				const ULONGLONG nowMs = GetTickCount64();
				if (nowMs - t.measuredAt >= 500) {
					t.measuredAt = nowMs;
					Measure(w, t);
				}
				if (t.measured && a_s.enabled) {
					// against the anchor fixed at measurement, never against a value this frame changes
					ox = std::clamp(ox, -t.baseVX, std::max(-t.baseVX, g_viewW - t.baseVX - t.vw));
					oy = std::clamp(oy, -t.baseVY, std::max(-t.baseVY, g_viewH - t.baseVY - t.vh));
				}
				const double wantX = t.baseX + ox, wantY = t.baseY + oy, wantSX = t.baseSX * scale, wantSY = t.baseSY * scale;
				const bool   atBase = ox == 0.0 && oy == 0.0 && scale == 1.0;
				if (!atBase && !t.pivotSet) {
					// grow and shrink about the element's own centre, as the Skyrim mod does
					if (const auto* pv = ue::At<double>(w, Off(WidgetClass(), "RenderTransformPivot"))) {
						t.pivotX = pv[0];
						t.pivotY = pv[1];
					}
					Call2(w, L"SetRenderTransformPivot", "Pivot", 0.5, 0.5);
					t.pivotSet = true;
				}
				if (std::abs(now.x - wantX) > eps || std::abs(now.y - wantY) > eps) {
					Call2(w, L"SetRenderTranslation", "Translation", wantX, wantY);
				}
				if (std::abs(now.sx - wantSX) > 1e-4 || std::abs(now.sy - wantSY) > 1e-4) {
					Call2(w, L"SetRenderScale", "Scale", wantSX, wantSY);
				}
				if (atBase && t.pivotSet) {
					Call2(w, L"SetRenderTransformPivot", "Pivot", t.pivotX, t.pivotY);   // back to the game's own
					t.pivotSet = false;
				}
				t.lastX = wantX;
				t.lastY = wantY;
				t.lastSX = wantSX;
				t.lastSY = wantSY;
				t.wrote = true;

				// hide: Hidden, and what the game had put back when it is shown again
				const bool   hide = a_s.enabled && e.hide;
				const auto   vis = Visibility(w);
				if (hide) {
					if (!t.hiddenByUs) {
						t.visibleBefore = vis;
						t.hiddenByUs = true;
						logger::info("hud: {} hidden", all[i].key);
					}
					if (vis != kHidden && vis != kCollapsed) {
						SetVisibility(w, kHidden);
					}
				} else if (t.hiddenByUs) {
					SetVisibility(w, t.visibleBefore);
					t.hiddenByUs = false;
					logger::info("hud: {} shown again", all[i].key);
				}

				// always visible: in gameplay only, for the elements the game fades or hides on its own
				const bool always = !hide && a_gameplay && all[i].fades && (a_s.alwaysVisible || e.alwaysVisible);
				if (always) {
					if (Opacity(w) < 0.999f) {
						SetOpacity(w, 1.0f);
					}
					if (const auto v = Visibility(w); v == kHidden || v == kCollapsed) {
						SetVisibility(w, kSelfHitTestInvisible);
					}
					bool      stopped = false;
					const int held = HoldSubtreeVisible(w, 0, stopped);
					if ((held > 0 || stopped) && !t.forced) {
						logger::info("hud: {} always visible - {} widget(s) held at full opacity{}", all[i].key, held, stopped ? ", its fade-out stopped" : "");
					}
					t.forced = true;
				} else if (t.forced) {
					t.forced = false;
					logger::info("hud: {} back to the game's own showing and hiding", all[i].key);
				}

				st.found = true;
				st.widget = t.name;
				st.baseX = t.baseX;
				st.baseY = t.baseY;
				st.baseScale = t.baseSX;
				st.x = wantX;
				st.y = wantY;
				st.scale = wantSX;
				st.opacity = Opacity(w);
				st.visibility = Visibility(w);
				st.forcedVisible = t.forced;
				st.measured = t.measured;
				st.vx = t.vx;
				st.vy = t.vy;
				st.vw = t.vw;
				st.vh = t.vh;
				st.viewW = g_viewW;
				st.viewH = g_viewH;
				st.baseVX = t.baseVX;
				st.baseVY = t.baseVY;
				std::scoped_lock l(g_lock);
				g_status[i] = st;
			}
		}
	}

	void Tick()
	{
		if (!ue::SelfCheck()) {
			return;   // the property layout is not proven yet
		}
		auto* layout = Layout();
		{
			std::scoped_lock l(g_lock);
			g_hudFound = layout != nullptr;
		}
		if (!layout) {
			return;
		}
		// find the elements again every 2 s while one is missing or gone (the HUD is built late and rebuilt on a load)
		bool gap = false;
		for (const auto& t : g_el) {
			gap |= t.widget.Get() == nullptr;
		}
		const ULONGLONG now = GetTickCount64();
		if (gap && now - g_lastFind >= 2000) {
			g_lastFind = now;
			Find(layout);
		}
		auto*      im = RE::InterfaceManager::GetInstance(false, false);
		const bool gameplay = im && im->menuMode == 1;   // Oblivion Remastered: 1 is gameplay (logic library, menuMode entry)
		Apply(settings::Snapshot(), gameplay);
	}

	bool OffsetRange(const ElementStatus& a_st, double a_withX, double a_withY, double& a_minX, double& a_maxX, double& a_minY, double& a_maxY)
	{
		if (!a_st.measured || a_st.viewW <= 0.0 || a_st.viewH <= 0.0) {
			return false;
		}
		// baseVX is the rectangle's left with NO offset at all; what it moves with is added, the slider's own part is not
		const double leftAtZero = a_st.baseVX + a_withX;   // the rectangle's left with the slider at 0
		const double topAtZero = a_st.baseVY + a_withY;
		a_minX = -leftAtZero;
		a_maxX = std::max(a_minX, a_st.viewW - leftAtZero - a_st.vw);
		a_minY = -topAtZero;
		a_maxY = std::max(a_minY, a_st.viewH - topAtZero - a_st.vh);
		return true;
	}

	std::pair<double, double> MoveWithOffset(const settings::Values& a_s, std::size_t a_i)
	{
		if (a_i >= a_s.elements.size()) return { 0.0, 0.0 };
		const auto [x, y] = Offset(a_s, a_i);
		return { x - a_s.elements[a_i].x, y - a_s.elements[a_i].y };
	}

	bool HudFound()
	{
		std::scoped_lock l(g_lock);
		return g_hudFound;
	}

	std::vector<ElementStatus> Statuses()
	{
		std::scoped_lock l(g_lock);
		return g_status;
	}

	json State()
	{
		const auto  st = Statuses();
		const auto& all = elements::All();
		json        els = json::object();
		for (std::size_t i = 0; i < all.size(); ++i) {
			const auto& s = st[i];
			els[all[i].key] = s.found ? json{ { "found", true }, { "widget", s.widget }, { "base", { s.baseX, s.baseY, s.baseScale } },
												{ "now", { s.x, s.y, s.scale } }, { "opacity", s.opacity }, { "visibility", s.visibility },
												{ "forced_visible", s.forcedVisible }, { "measured", s.measured },
												{ "rect", { s.vx, s.vy, s.vw, s.vh } }, { "viewport", { s.viewW, s.viewH } } }
									  : json{ { "found", false } };
		}
		return { { "hud_found", HudFound() }, { "elements", els } };
	}
}
