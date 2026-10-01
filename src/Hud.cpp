#include "Hud.h"

#include "Settings.h"
#include "Strings.h"

#include <array>
#include <unordered_map>
#include "Ue.h"

namespace hud
{
	namespace
	{
		constexpr const wchar_t* kLayoutClass = L"/Game/UI/WBP_PrimaryGameLayout.WBP_PrimaryGameLayout_C";   // the root of all the game's UI

		// ESlateVisibility
		constexpr std::uint8_t kVisible = 0, kCollapsed = 1, kHidden = 2, kSelfHitTestInvisible = 4;

		struct IndicatorFlash   // a damage arc: its direction relative to the view, when it was hit, how strong
		{
			float     angle = 0.0f;
			ULONGLONG at = 0;
			float     strength = 1.0f;
		};

		struct Tracked
		{
			// the indicators (createNative): their marks, and what each was last given
			std::vector<UE::UObject*>   marks;
			std::vector<float>          markOp, markAng, markColour, markSize;
			float                       ringRadius = -1.0f; // the radius the marks were placed at
			bool                        artSet = false;     // the game's textures are on the marks
			ULONGLONG                   artTriedAt = 0;
			UE::UObject*                area = nullptr;     // the transparent image that gives the ring its size
			float                       lastHealth = -1.0f;
			std::vector<IndicatorFlash> flashes;
			ULONGLONG                   hitLoggedAt = 0;
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
			// the drawn rectangle (the union of the visible images, text blocks and progress bars), for the collision
			bool        drawn = false;
			double      dx = 0, dy = 0, dw = 0, dh = 0, baseDX = 0, baseDY = 0;
			// the preview: shown by this code while the page is open, with the game's state to put back
			bool         shownByUs = false;
			ULONGLONG    previewCalledAt = 0;
			bool         created = false;        // made by this mod (createClass)
			// a retainer box above the element whose material hides it (the enemy's health bar: M_UI_RetainerTransform's
			// Opacity at 0 with every widget reporting visible) - raised for the preview, put back after
			UE::UObject* veilMaterial = nullptr;
			std::wstring veilParam;
			float        veilWas = 0.0f;
			bool         veiled = false;
			int          shownLevel = -1;        // the level the created text shows
			ULONGLONG    levelCheckedAt = 0;
			std::vector<std::pair<UE::UObject**, UE::UObject*>> mutedSounds;
			// the hold (always visible / the preview): what it changed, put back when it ends
			bool         held = false, previewWas = false;
			std::uint8_t visBeforeHold = kSelfHitTestInvisible;
			float        opacityBeforeHold = 1.0f;
			std::vector<std::pair<UE::UObject*, float>>        raisedOpacities;   // child widgets whose opacity the hold raised
			std::vector<std::pair<UE::UObject*, UE::UObject*>> stoppedFades;      // (user widget, FadeOut animation) the hold stopped   // the sound-event slots cleared for the preview, with what they held
			std::uint8_t visBeforePreview = kSelfHitTestInvisible;
			float        opacityBeforePreview = 1.0f;
			// "Fill from" (2026-09-29): the bar's progress image, the material instance the game gave it, and what is on it now
			UE::UObject* fillImage = nullptr;
			bool         fillImageLooked = false;
			UE::UObject* fillOriginal = nullptr;
			UE::UObject* fillMid = nullptr;
			int          fillApplied = 0;
			ULONGLONG    fillCheckedAt = 0;
			// the hold's calls (holdOn) run again when the retainer veil the game fades drops (the enemy bar, 2026-09-29)
			ULONGLONG    holdCalledAt = 0, veilCheckedAt = 0, holdCheckedAt = 0;
			float        followedOpacity = -1.0f;   // the created Level gauge: the opacity it was last given from the Health bar
			UE::UObject* veilNear = nullptr;   // the nearest retainer box's material (above or below the element), looked up once
			bool         veilLooked = false;
			float        veilNow = 1.0f;
			double       pivotAppliedX = 0.5;
			float        shownProgress = -1.0f;   // the level gauge's bar
			double       sizeBaseW = 0, sizeBaseH = 0, sizeAppliedX = 0, sizeAppliedY = 0;   // sizeImage: the image's own size, and the override written
			// moveViaSlot: the slot's padding (Left, Top, Right, Bottom) - the game's own, and what this mod wrote last
			bool        havePadBase = false, padWrote = false;
			float       basePad[4]{}, lastPad[4]{};
			double      slotDx = 0, slotDy = 0;   // the shift applied through the slot (the geometry moved by it, the transform did not)
		};
		double g_viewW = 0, g_viewH = 0;
		std::atomic<ULONGLONG> g_pageDrawnAt{ 0 };   // the preview runs while the page keeps saying it is drawn

		std::vector<Tracked> g_el(elements::Count());
		void SetVisibility(UE::UObject* a_w, std::uint8_t a_v);   // defined below; ReleaseHold uses them first
		UE::UObject* DynamicMaterial(UE::UObject* a_image);         // defined below (the fill); the level gauge uses it first
		UE::UObject* CreateIndicator(const elements::Element& a_el, struct Tracked& a_t);   // defined below (the two indicators)
		int HoldSubtreeVisible(UE::UObject* a_widget, int a_depth, bool& a_stoppedFade, Tracked* a_t = nullptr);
		void SetOpacity(UE::UObject* a_w, float a_o);
		ue::Handle           g_layout;

		// ---- neighbours ([General] bNoOverlap): an element's edge stops at another element's edge ----------------
		// (the owner, 2026-09-29: "line up the three bars on top of each other without having to be precise to the
		// decimal point and it'll just automatically stop once the borders of the widgets meet")
		struct Rect
		{
			bool   valid = false;
			double l = 0, t = 0, r = 0, b = 0;
		};

		// does element a_j move with a_i (directly or through the chain), or do both move as one in the group?
		bool MovesWith(const settings::Values& a_s, std::size_t a_j, std::size_t a_i)
		{
			const auto& all = elements::All();
			if (a_s.group.Has(all[a_j].key) && a_s.group.Has(all[a_i].key)) return true;
			std::size_t cur = a_j;
			for (int hop = 0; hop < 8; ++hop) {
				std::string with = a_s.elements[cur].moveWith;
				if (with.empty()) return false;
				const int k = elements::IndexOf(with);
				if (k < 0) return false;
				if (static_cast<std::size_t>(k) == a_i) return true;
				cur = static_cast<std::size_t>(k);
			}
			return false;
		}

		// The travel a_i's rectangle (a_me, where it is NOW) may take before an edge meets a neighbour's: the left may go
		// down to a_lMin and up to a_lMax, the top likewise. A neighbour whose vertical band overlaps a_me and sits to
		// its left bounds the leftward travel at its right edge, one to its right bounds the rightward travel, and the
		// same up and down. A neighbour a_me already overlaps is ignored (else the two could never come apart), as are
		// hidden ones and the ones that move with a_i (they move together). Unmeasured ones do not exist.
		void NeighbourLimits(const settings::Values& a_s, std::size_t a_i, const Rect& a_me, const std::vector<Rect>& a_rects,
			double& a_lMin, double& a_lMax, double& a_tMin, double& a_tMax)
		{
			constexpr double eps = 0.5;
			a_lMin = -1e9; a_lMax = 1e9; a_tMin = -1e9; a_tMax = 1e9;
			const double w = a_me.r - a_me.l, h = a_me.b - a_me.t;
			for (std::size_t j = 0; j < a_rects.size(); ++j) {
				const Rect& o = a_rects[j];
				if (j == a_i || !o.valid || a_s.elements[j].hide || MovesWith(a_s, j, a_i)) continue;
				// only a widget the owner has placed is an obstacle: a game-placed one may be hidden in a way nothing readable
				// shows (the enemy health bar behind a retainer box's material) and must not box the others in (2026-09-29)
				const auto& oe = a_s.elements[j];
				if (oe.x == 0.0f && oe.y == 0.0f && oe.scale == 1.0f && oe.stretchX == 1.0f && oe.stretchY == 1.0f) continue;
				const bool bandY = o.t < a_me.b - eps && o.b > a_me.t + eps;   // side by side
				const bool bandX = o.l < a_me.r - eps && o.r > a_me.l + eps;   // one above the other
				if (bandY) {
					if (o.r <= a_me.l + eps) a_lMin = std::max(a_lMin, o.r);          // to the left
					else if (o.l >= a_me.r - eps) a_lMax = std::min(a_lMax, o.l - w);   // to the right
				}
				if (bandX) {
					if (o.b <= a_me.t + eps) a_tMin = std::max(a_tMin, o.b);          // above
					else if (o.t >= a_me.b - eps) a_tMax = std::min(a_tMax, o.t - h);   // below
				}
			}
		}

		// Snap ([General] bSnapEdges): the nearest edge of another PLACED widget's art within a_dist of one of a_me's edges,
		// per axis - a_me's left and right against the other's left and right, top and bottom against top and bottom. The
		// smallest move wins on each axis; 0 when nothing is near. Hidden neighbours, the ones that move with a_i and
		// the game-placed ones are left out, as for the collision.
		void SnapDeltas(const settings::Values& a_s, std::size_t a_i, const Rect& a_me, const std::vector<Rect>& a_rects, double a_dist, double& a_dx, double& a_dy)
		{
			a_dx = 0.0; a_dy = 0.0;
			double bestX = a_dist + 1e-6, bestY = a_dist + 1e-6;
			for (std::size_t j = 0; j < a_rects.size(); ++j) {
				const Rect& o = a_rects[j];
				if (j == a_i || !o.valid || a_s.elements[j].hide || MovesWith(a_s, j, a_i)) continue;
				const auto& oe = a_s.elements[j];
				if (oe.x == 0.0f && oe.y == 0.0f && oe.scale == 1.0f && oe.stretchX == 1.0f && oe.stretchY == 1.0f) continue;
				for (const double mine : { a_me.l, a_me.r }) {
					for (const double theirs : { o.l, o.r }) {
						const double d = theirs - mine;
						if (std::abs(d) < bestX) { bestX = std::abs(d); a_dx = d; }
					}
				}
				for (const double mine : { a_me.t, a_me.b }) {
					for (const double theirs : { o.t, o.b }) {
						const double d = theirs - mine;
						if (std::abs(d) < bestY) { bestY = std::abs(d); a_dy = d; }
					}
				}
			}
		}

		std::vector<Rect> TrackedRects()
		{
			std::vector<Rect> out(g_el.size());
			for (std::size_t j = 0; j < g_el.size(); ++j) {
				const auto& t = g_el[j];
				if (t.widget.Get() && t.measured && t.drawn) out[j] = { true, t.dx, t.dy, t.dx + t.dw, t.dy + t.dh };
			}
			return out;
		}
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

		// a UClass by its short name (the widget blueprints' generated classes are not reachable by a path this code knows)
		UE::UClass* ClassByName(const char* a_name)
		{
			static std::unordered_map<std::string, UE::UClass*> cache;
			if (auto it = cache.find(a_name); it != cache.end() && it->second && ue::IsLive(it->second)) return it->second;
			auto* arr = UE::FUObjectArray::GetSingleton();
			if (!arr) return nullptr;
			UE::UClass* found = nullptr;
			arr->LockInternalArray();
			const std::int32_t n = arr->GetObjectArrayNum();
			for (std::int32_t i = 0; i < n && !found; ++i) {
				auto* item = arr->IndexToObject(i);
				auto* o = item ? reinterpret_cast<UE::UObject*>(item->object) : nullptr;
				if (o && o->GetClass() && ue::NameOf(o->GetClass()) == "WidgetBlueprintGeneratedClass" && ue::NameOf(o) == a_name) found = static_cast<UE::UClass*>(o);
			}
			arr->UnlockInternalArray();
			cache[a_name] = found;
			return found;
		}

		// The Level element: the game's own level-up gauge (WBP_ModernHud_LevelUpGauge_C) made by this mod. Its PlayerLevelText
		// rich text gets the player's level, its AltarProgressBar image's material the progress to the next level (from the
		// HUD's view model, SkillProgression.PlayerLevelProgress), at most once a second and only when either changed.
		float LevelProgress()
		{
			static ue::Handle vm;
			static ULONGLONG  scanAt = 0;
			auto* v = vm.Get();
			if (!v && GetTickCount64() - scanAt >= 5000) {
				scanAt = GetTickCount64();
				v = ue::FirstOf(ue::Class(L"/Script/Altar.VHUDMainViewModel"));
				vm.Set(v);
			}
			if (!v) return -1.0f;
			static auto* st = UE::StaticFindObject<UE::UStruct>(nullptr, nullptr, L"/Script/Altar.ModernSkillProgression");
			const auto off = Off(v->GetClass(), "SkillProgression");
			const auto inner = st ? Off(st, "PlayerLevelProgress") : -1;
			if (off < 0 || inner < 0) {
				static bool warned = false;
				if (!warned) { warned = true; logger::warn("hud: the level progress is not readable (SkillProgression at {}, PlayerLevelProgress at {}) - the level gauge's bar stays as it is", off, inner); }
				return -1.0f;
			}
			const float* p = ue::At<float>(v, off + inner);
			const float  fromHud = p ? std::clamp(*p, 0.0f, 1.0f) : -1.0f;
			// the HUD's view model is only given the progress on a skill event: until then the player's own count of major
			// skill advances since the last level (ten make a level) stands in (the gauge came up empty, 2026-09-29)
			auto* player = RE::PlayerCharacter::GetSingleton();
			const bool placed = player && player->parentCell != nullptr;   // no character before a save is loaded
			const float fromPlayer = placed ? std::clamp(static_cast<float>(player->skillAdvanceCount) / 10.0f, 0.0f, 1.0f) : -1.0f;
			static bool logged = false;
			if (!logged && fromHud >= 0.0f) { logged = true; logger::info("hud: level progress - the HUD says {:.3f}, the player's skill advances {}", fromHud, player ? player->skillAdvanceCount : -1); }
			return fromHud > 0.0f ? fromHud : fromPlayer;
		}

		void SetLevelGauge(UE::UObject* a_w, Tracked& a_t)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->parentCell) return;   // no character yet (the main menu): nothing to show, nothing to ask it
			const int level = static_cast<int>(player->GetLevel());
			const float progress = LevelProgress();
			if (auto* bar = ObjProp(a_w, "AltarProgressBar"); bar && progress >= 0.0f && std::abs(progress - a_t.shownProgress) > 0.004f) {
				if (auto* mid = DynamicMaterial(bar)) {
					for (const wchar_t* name : { L"Progress", L"OldProgress", L"AnimatedProgress", L"DelayedAnimatedProgress" }) {
						ue::Call set(mid, L"SetScalarParameterValue");
						if (!set || !set.At("ParameterName")) break;
						new (set.At("ParameterName")) UE::FName(name, UE::EFindName::Add);
						set.Set<float>("Value", progress);
						set.Run();
					}
					a_t.shownProgress = progress;
				}
			}
			if (level == a_t.shownLevel) return;
			if (auto* text = ObjProp(a_w, "PlayerLevelText")) a_w = text;   // the gauge's level number; a plain text block otherwise
			static UE::UObject* textLib = nullptr;
			if (!textLib) {
				auto* cls = UE::StaticFindObject<UE::UClass>(nullptr, nullptr, L"/Script/Engine.KismetTextLibrary");
				textLib = cls ? cls->GetDefaultObject(false) : nullptr;
			}
			ue::Call conv(textLib, L"Conv_StringToText");
			ue::Call set(a_w, L"SetText");
			void* in = conv ? conv.At("InString") : nullptr;
			void* ret = conv ? conv.At("ReturnValue") : nullptr;
			void* out = set ? set.At("InText") : nullptr;
			if (!in || !ret || !out) return;
			std::string text = std::to_string(level);   // the gauge's own "Lvl" label stands before it
			new (in) UE::FString(ue::Widen(text).c_str());
			conv.Run();
			std::memcpy(out, ret, 24);
			set.Run();
			a_t.shownLevel = level;
		}

		// an element the game has no widget for, made from one of its own classes and put on the layout's root Overlay
		UE::UObject* CreateElement(UE::UObject* a_layout, const elements::Element& a_el, Tracked& a_t)
		{
			auto* cls = ClassByName(a_el.createClass);
			// the HUD's own layer (WBP_ModernHud_PrimaryLayout), reached through the Health widget's outers: on the root of all the
			// game's UI the gauge showed at the main menu and over menus (the owner, 2026-09-29); the HUD layer is hidden with the HUD
			UE::UObject* hudLayer = nullptr;
			if (const int h = elements::IndexOf("Health"); h >= 0) {
				for (UE::UObject* o = g_el[static_cast<std::size_t>(h)].widget.Get(); o && !hudLayer; o = o->GetOuter()) {
					if (ue::NameOf(o->GetClass()) == "WBP_ModernHud_PrimaryLayout_C") hudLayer = o;
				}
			}
			if (!hudLayer) return nullptr;   // not yet: the HUD is built later, and Find() asks again every 2 s
			auto* root = ObjProp(ObjProp(hudLayer, "WidgetTree"), "RootWidget");
			if (!cls || !root) return nullptr;
			auto* lib = UE::StaticFindObject<UE::UClass>(nullptr, nullptr, L"/Script/UMG.WidgetBlueprintLibrary");
			ue::Call create(lib ? lib->GetDefaultObject(false) : nullptr, L"Create");
			if (!create || ue::Dying(a_layout)) return nullptr;
			create.Set("WorldContextObject", a_layout);
			create.Set("WidgetType", cls);
			if (!create.RunGuarded()) return nullptr;
			auto** made = static_cast<UE::UObject**>(create.At("ReturnValue"));
			auto* w = made ? *made : nullptr;
			if (!w) return nullptr;
			ue::Call add(root, L"AddChild");
			if (!add) { logger::warn("hud: {}: the layout's root {} takes no child - not created", a_el.key, ue::NameOf(root->GetClass())); return nullptr; }
			add.Set("Content", w);
			add.Run();
			auto** slotp = static_cast<UE::UObject**>(add.At("ReturnValue"));
			if (auto* slot = slotp ? *slotp : nullptr) {
				for (const auto& [fn, arg] : std::initializer_list<std::pair<const wchar_t*, const char*>>{ { L"SetHorizontalAlignment", "InHorizontalAlignment" }, { L"SetVerticalAlignment", "InVerticalAlignment" } }) {
					// top-left: the offset moves it from there. EHorizontalAlignment / EVerticalAlignment: 0 is FILL (the text's
					// geometry was the whole screen and its sliders had no range, 2026-09-29), 1 is Left / Top
					ue::Call set(slot, fn);
					if (void* p = set.At(arg)) { *static_cast<std::uint8_t*>(p) = 1; set.Run(); }
				}
			}
			SetVisibility(w, kSelfHitTestInvisible);
			a_t.created = true;
			// the level-up gauge as a level display: no skill text, no level-up icon, its row at full opacity
			if (auto* info = ObjProp(w, "InfoText")) SetVisibility(info, kCollapsed);
			{
				ue::Call icon(w, L"ToggleLevelUpIconVisibility");
				if (icon && icon.At("Visible")) { icon.Set<bool>("Visible", false); icon.Run(); }
			}
			bool stopped = false;
			HoldSubtreeVisible(w, 0, stopped);
			SetLevelGauge(w, a_t);
			logger::info("hud: {} created from {} on the layout's {}", a_el.key, a_el.createClass, ue::NameOf(root->GetClass()));
			return w;
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
				if (!widget && (all[i].createClass || all[i].createNative)) {
					if (t.created && t.widget.Get()) { ++found; continue; }   // ours, still alive
					Tracked fresh{};
					widget = all[i].createNative ? CreateIndicator(all[i], fresh) : CreateElement(a_layout, all[i], fresh);
					if (widget) { t = fresh; t.widget.Set(widget); t.name = std::format("{} ({}, made by this mod)", all[i].names[0], all[i].createNative ? "plain UMG widgets" : all[i].createClass); ++found; }
					else { missing += std::string(missing.empty() ? "" : ", ") + all[i].key; }
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

		// The element's offset through its Overlay slot's padding (moveViaSlot). Centre alignment: a Left padding of 2*dx
		// moves the content right by dx (the box grows on one side, the centre shifts by half), Right by -dx, Top / Bottom
		// the same vertically. The game's own padding is the base, followed the way the transform is: a value this mod
		// did not write is the game's, and the offset rides on it. False when the slot is not an Overlay slot (the
		// caller then falls back to the render transform).
		bool ApplySlotShift(UE::UObject* a_w, Tracked& a_t, double a_dx, double a_dy)
		{
			auto* slot = ObjProp(a_w, "Slot");
			if (!slot || ue::NameOf(slot->GetClass()) != "OverlaySlot") return false;
			const auto off = Off(slot->GetClass(), "Padding");
			if (off < 0) return false;
			const float* now = ue::At<float>(slot, off);   // FMargin: Left, Top, Right, Bottom
			const float eps = 0.01f;
			bool same = a_t.padWrote;
			for (int k = 0; k < 4 && same; ++k) same = std::abs(now[k] - a_t.lastPad[k]) <= eps;
			if (!a_t.havePadBase || !same) {
				std::memcpy(a_t.basePad, now, sizeof(a_t.basePad));   // the game's own padding (or its change)
				a_t.havePadBase = true;
			}
			float want[4] = { a_t.basePad[0], a_t.basePad[1], a_t.basePad[2], a_t.basePad[3] };
			if (a_dx >= 0.0) want[0] += static_cast<float>(2.0 * a_dx); else want[2] += static_cast<float>(-2.0 * a_dx);
			if (a_dy >= 0.0) want[1] += static_cast<float>(2.0 * a_dy); else want[3] += static_cast<float>(-2.0 * a_dy);
			bool differs = false;
			for (int k = 0; k < 4 && !differs; ++k) differs = std::abs(now[k] - want[k]) > eps;
			if (differs) {
				ue::Call c(slot, L"SetPadding");
				if (void* p = c.At("InPadding")) {
					std::memcpy(p, want, sizeof(want));
					c.Run();
				}
			}
			std::memcpy(a_t.lastPad, want, sizeof(want));
			a_t.padWrote = true;
			a_t.slotDx = a_dx;
			a_t.slotDy = a_dy;
			return true;
		}

		// the preview calls (elements::PreviewCall): the widget's own reflected functions with the given parameters; a text
		// parameter is an FText the engine makes from the translated string (KismetTextLibrary::Conv_StringToText)
		void RunPreviewCalls(UE::UObject* a_w, const std::vector<elements::PreviewCall>& a_calls, const char* a_key)
		{
			static UE::UObject* textLib = nullptr;
			if (!textLib) {
				auto* cls = UE::StaticFindObject<UE::UClass>(nullptr, nullptr, L"/Script/Engine.KismetTextLibrary");
				textLib = cls ? cls->GetDefaultObject(false) : nullptr;
			}
			for (const auto& call : a_calls) {
				UE::UObject* target = a_w;
				if (call.outerClass) {   // the nearest outer user widget of that class (the element's owner)
					target = nullptr;
					for (UE::UObject* o = a_w ? a_w->GetOuter() : nullptr; o && !target; o = o->GetOuter()) {
						if (Wide(ue::NameOf(o->GetClass())) == call.outerClass) target = o;
					}
					if (!target) {
						logger::debug("preview: {} has no outer {}", a_key, ue::Utf8FromWide(call.outerClass));
						continue;
					}
				}
				if (call.innerProp) {   // a widget named by an object property of the element (or of the outer just found)
					target = ObjProp(target, call.innerProp);
					if (!target) {
						logger::debug("preview: {} has no {}", a_key, call.innerProp);
						continue;
					}
				}
				ue::Call c(target, call.fn);
				if (!c) {
					logger::debug("preview: {} has no {}", a_key, ue::Utf8FromWide(call.fn));
					continue;
				}
				bool ok = true;
				for (const auto& arg : call.args) {
					void* p = c.At(arg.name);
					if (!p) { ok = false; break; }
					switch (arg.kind) {
					case elements::PreviewArg::kBool: static_cast<bool*>(p)[arg.at] = arg.b; break;
					case elements::PreviewArg::kDouble: std::memcpy(p, &arg.d, sizeof(double)); break;
					case elements::PreviewArg::kObjects:
					case elements::PreviewArg::kDoubles: {
						// a TArray parameter {data, num, max}: the buffer comes from the engine's allocator, because ProcessEvent
						// destroys its copy of the parameters after the call and frees the buffer with FMemory::Free
						struct Arr { void* data; std::int32_t num, max; } arr{ nullptr, 0, 0 };
						if (arg.kind == elements::PreviewArg::kObjects) {
							std::vector<UE::UObject*> objs;
							for (const auto* path : arg.objects) {
								if (auto* o = UE::StaticFindObject<UE::UObject>(nullptr, nullptr, path)) objs.push_back(o);
								else logger::debug("preview: {} - {} is not loaded", a_key, ue::Utf8FromWide(path));
							}
							if (!objs.empty()) {
								arr.data = UE::FMemory::Malloc(objs.size() * sizeof(UE::UObject*), 8);
								if (arr.data) { std::memcpy(arr.data, objs.data(), objs.size() * sizeof(UE::UObject*)); arr.num = arr.max = static_cast<std::int32_t>(objs.size()); }
							}
						} else if (!arg.doubles.empty()) {
							arr.data = UE::FMemory::Malloc(arg.doubles.size() * sizeof(double), 8);
							if (arr.data) { std::memcpy(arr.data, arg.doubles.data(), arg.doubles.size() * sizeof(double)); arr.num = arr.max = static_cast<std::int32_t>(arg.doubles.size()); }
						}
						std::memcpy(p, &arr, sizeof(arr));
						break;
					}
					case elements::PreviewArg::kText: {
						ue::Call conv(textLib, L"Conv_StringToText");
						void* in = conv ? conv.At("InString") : nullptr;
						void* ret = conv ? conv.At("ReturnValue") : nullptr;
						if (!in || !ret) { ok = false; break; }
						const std::string text = strings::Get(arg.trKey, arg.english);
						new (in) UE::FString(ue::Widen(text).c_str());   // destroyed by ProcessEvent with the other parameters
						conv.Run();
						std::memcpy(p, ret, 24);
						break;
					}
					}
				}
				if (ok) c.Run();
				else logger::debug("preview: {}'s {} has not the parameters this code expects", a_key, ue::Utf8FromWide(call.fn));
			}
		}

		// The widget's Wwise sound events (AkAudioEvent object properties, on it and on its child user widgets) cleared so the
		// preview's show calls make no noise; Unmute puts them back.
		void MuteSounds(UE::UObject* a_w, Tracked& a_t, int a_depth = 0)
		{
			if (!a_w || a_depth > 3) return;
			auto* cls = a_w->GetClass();
			for (const auto off : ue::ObjectPropertiesOfClass(cls, "AkAudioEvent")) {
				auto** slot = ue::At<UE::UObject*>(a_w, off);
				if (slot && *slot) {
					a_t.mutedSounds.emplace_back(slot, *slot);
					*slot = nullptr;
				}
			}
			// the child user widgets it names (a notification's inner prefab, a bar's status bar)
			if (UserWidgetClass() && cls->IsChildOf(UserWidgetClass())) {
				auto* root = ObjProp(ObjProp(a_w, "WidgetTree"), "RootWidget");
				std::vector<UE::UObject*> stack{ root };
				int seen = 0;
				while (!stack.empty() && ++seen < 200) {
					auto* w = stack.back();
					stack.pop_back();
					if (!w) continue;
					auto* wc = w->GetClass();
					if (wc->IsChildOf(UserWidgetClass())) {
						MuteSounds(w, a_t, a_depth + 1);   // a nested user widget: its own sound slots, and its tree
						continue;
					}
					if (PanelClass() && wc->IsChildOf(PanelClass())) {
						auto* slots = ue::At<RawArray>(w, Off(wc, "Slots"));
						for (std::int32_t i = 0; slots && slots->data && i < slots->num && i < 256; ++i) stack.push_back(ObjProp(slots->data[i], "Content"));
					} else if (Off(wc, "Content") >= 0) {
						stack.push_back(ObjProp(w, "Content"));
					}
				}
			}
		}

		// everything a hold changed, put back: the raised child opacities, the stopped fade-outs played again (the game's
		// own fade takes the element away as it would have), the root's visibility and opacity
		void ReleaseHold(UE::UObject* a_w, Tracked& a_t)
		{
			for (auto& [child, opacity] : a_t.raisedOpacities) {
				if (ue::IsLive(child)) SetOpacity(child, opacity);
			}
			a_t.raisedOpacities.clear();
			for (auto& [owner, fade] : a_t.stoppedFades) {
				if (!ue::IsLive(owner)) continue;
				ue::Call play(owner, L"PlayAnimation");
				if (!play) continue;
				play.Set("InAnimation", fade);
				play.Set<float>("StartAtTime", 0.0f);
				play.Set<std::int32_t>("NumLoopsToPlay", 1);
				play.Set<float>("PlaybackSpeed", 1.0f);
				play.Run();
			}
			a_t.stoppedFades.clear();
			if (!a_t.hiddenByUs) SetVisibility(a_w, a_t.visBeforeHold);
			SetOpacity(a_w, a_t.opacityBeforeHold);
		}

		// The retainer box's material veil (2026-09-29: the enemy's health bar reported Visible with opacity 1 on every widget
		// and ancestor, hidden only by an AnimatableRetainerBox's dynamic material, found live: M_UI_RetainerTransform's
		// one scalar parameter, Opacity, 0 while hidden). The nearest such ancestor's material has that parameter raised
		// to 1 while the element is previewed; the value it had comes back after. The parameter's name is read off the
		// material's own first scalar parameter, Opacity when the material has none.
		void RaiseVeil(UE::UObject* a_w, Tracked& a_t)
		{
			if (a_t.veiled) return;
			UE::UObject* cur = a_w;
			for (int up = 0; up < 6 && cur; ++up) {
				auto* slot = ObjProp(cur, "Slot");
				auto* parent = slot ? ObjProp(slot, "Parent") : nullptr;
				if (!parent) break;
				if (ue::NameOf(parent->GetClass()) == "AnimatableRetainerBox") {
					auto* mid = ObjProp(parent, "EffectMaterial");
					if (!mid) break;
					std::wstring param = L"Opacity";
					if (const auto off = Off(mid->GetClass(), "ScalarParameterValues"); off >= 0) {
						const auto* arr = ue::At<RawArray>(mid, off);   // FScalarParameterValue: FMaterialParameterInfo { FName Name ... } first
						if (arr && arr->data && arr->num > 0) {
							const auto& name = *reinterpret_cast<const UE::FName*>(arr->data);
							const std::string s = ue::Utf8FromWide(UE::GetData(name.ToString()));
							if (!s.empty()) param = ue::Widen(s);
						}
					}
					ue::Call get(mid, L"K2_GetScalarParameterValue");
					ue::Call set(mid, L"SetScalarParameterValue");
					if (!get || !set) break;
					new (get.At("ParameterName")) UE::FName(param.c_str(), UE::EFindName::Add);
					get.Run();
					const float* was = static_cast<const float*>(get.At("ReturnValue"));
					a_t.veilWas = was ? *was : 0.0f;
					new (set.At("ParameterName")) UE::FName(param.c_str(), UE::EFindName::Add);
					set.Set<float>("Value", 1.0f);
					set.Run();
					a_t.veilMaterial = mid;
					a_t.veilParam = param;
					a_t.veiled = true;
					logger::info("preview: a retainer box's material veils this element - its {} raised from {:.2f} to 1", ue::Utf8FromWide(param.c_str()), a_t.veilWas);
					break;
				}
				cur = parent;
			}
		}

		// the nearest retainer box's material veil (an ancestor, or a descendant for the top-stats block whose retainer
		// holds the enemy bar), found once; its Opacity parameter read at most twice a second - 1 when there is none
		UE::UObject* FindRetainerMaterial(UE::UObject* a_w, int a_depth = 0)
		{
			if (!a_w || a_depth > 6) return nullptr;
			auto* cls = a_w->GetClass();
			if (ue::NameOf(cls) == "AnimatableRetainerBox") return ObjProp(a_w, "EffectMaterial");
			if (UserWidgetClass() && cls->IsChildOf(UserWidgetClass())) {
				if (auto* m = FindRetainerMaterial(ObjProp(ObjProp(a_w, "WidgetTree"), "RootWidget"), a_depth + 1)) return m;
			}
			if (PanelClass() && cls->IsChildOf(PanelClass())) {
				auto* slots = ue::At<RawArray>(a_w, Off(cls, "Slots"));
				for (std::int32_t i = 0; slots && slots->data && i < slots->num && i < 64; ++i) {
					if (auto* m = FindRetainerMaterial(ObjProp(slots->data[i], "Content"), a_depth + 1)) return m;
				}
			} else if (Off(cls, "Content") >= 0) {
				if (auto* m = FindRetainerMaterial(ObjProp(a_w, "Content"), a_depth + 1)) return m;
			}
			return nullptr;
		}

		float VeilOpacity(UE::UObject* a_w, Tracked& a_t)
		{
			const ULONGLONG now = GetTickCount64();
			if (!a_t.veilLooked) {
				a_t.veilLooked = true;
				a_t.veilNear = FindRetainerMaterial(a_w);
				for (UE::UObject* cur = a_w; !a_t.veilNear && cur;) {   // or above
					auto* slot = ObjProp(cur, "Slot");
					auto* parent = slot ? ObjProp(slot, "Parent") : nullptr;
					if (parent && ue::NameOf(parent->GetClass()) == "AnimatableRetainerBox") a_t.veilNear = ObjProp(parent, "EffectMaterial");
					cur = parent;
				}
			}
			if (!a_t.veilNear) return 1.0f;
			if (now - a_t.veilCheckedAt >= 500) {
				a_t.veilCheckedAt = now;
				if (!ue::IsLive(a_t.veilNear)) { a_t.veilNear = nullptr; return 1.0f; }
				ue::Call get(a_t.veilNear, L"K2_GetScalarParameterValue");
				if (get && get.At("ParameterName")) {
					new (get.At("ParameterName")) UE::FName(L"Opacity", UE::EFindName::Add);
					get.Run();
					if (const auto* v = static_cast<const float*>(get.At("ReturnValue"))) a_t.veilNow = *v;
				}
			}
			return a_t.veilNow;
		}

		void LowerVeil(Tracked& a_t)
		{
			if (!a_t.veiled) return;
			a_t.veiled = false;
			if (a_t.veilMaterial && ue::IsLive(a_t.veilMaterial)) {
				ue::Call set(a_t.veilMaterial, L"SetScalarParameterValue");
				if (set) {
					new (set.At("ParameterName")) UE::FName(a_t.veilParam.c_str(), UE::EFindName::Add);
					set.Set<float>("Value", a_t.veilWas);
					set.Run();
				}
			}
			a_t.veilMaterial = nullptr;
		}

		void UnmuteSounds(Tracked& a_t)
		{
			for (auto& [slot, was] : a_t.mutedSounds) {
				if (slot && *slot == nullptr) *slot = was;
			}
			a_t.mutedSounds.clear();
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
		int HoldSubtreeVisible(UE::UObject* a_widget, int a_depth, bool& a_stoppedFade, Tracked* a_t)
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
						if (a_t) a_t->stoppedFades.emplace_back(a_widget, fade);
					}
				}
				held += HoldSubtreeVisible(ObjProp(ObjProp(a_widget, "WidgetTree"), "RootWidget"), a_depth + 1, a_stoppedFade, a_t);
			}
			if (PanelClass() && cls->IsChildOf(PanelClass())) {
				auto* slots = ue::At<RawArray>(a_widget, Off(cls, "Slots"));
				for (std::int32_t i = 0; slots && slots->data && i < slots->num && i < 64; ++i) {
					held += HoldSubtreeVisible(ObjProp(slots->data[i], "Content"), a_depth + 1, a_stoppedFade, a_t);
				}
			}
			if (a_depth > 0 && Opacity(a_widget) < 0.999f) {
				if (a_t && std::ranges::none_of(a_t->raisedOpacities, [&](const auto& p) { return p.first == a_widget; })) {
					a_t->raisedOpacities.emplace_back(a_widget, Opacity(a_widget));   // the first value seen is the game's
				}
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

		// a widget's painted rectangle in layout units (its cached geometry through the Slate library); the size is the
		// layout size times a_scale (the render scale the geometry was painted with)
		bool MeasureRect(UE::UObject* a_w, double a_scaleX, double a_scaleY, double& a_x, double& a_y, double& a_wOut, double& a_hOut)
		{
			ue::Call geom(a_w, L"GetCachedGeometry");
			if (!geom || !SlateLib()) return false;
			geom.Run();
			const void* g = geom.At("ReturnValue");
			const auto  gsize = static_cast<std::size_t>(reinterpret_cast<UE::UStruct*>(geom.Function())->propertiesSize);
			if (!g || gsize == 0 || gsize > 128) return false;
			ue::Call toView(SlateLib(), L"LocalToViewport");
			ue::Call size(SlateLib(), L"GetLocalSize");
			if (!toView || !size || !toView.At("Geometry") || !size.At("Geometry")) return false;
			std::memcpy(toView.At("Geometry"), g, gsize);
			std::memcpy(size.At("Geometry"), g, gsize);
			toView.Set("WorldContextObject", a_w);
			const double zero[2] = { 0.0, 0.0 };
			if (void* lc = toView.At("LocalCoordinate")) std::memcpy(lc, zero, sizeof(zero));
			if (ue::Dying(a_w) || !toView.RunGuarded() || !size.RunGuarded()) return false;
			const auto* vp = static_cast<const double*>(toView.At("ViewportPosition"));
			const auto* sz = static_cast<const double*>(size.At("ReturnValue"));
			if (!vp || !sz) return false;
			a_x = vp[0];
			a_y = vp[1];
			a_wOut = sz[0] * a_scaleX;
			a_hOut = sz[1] * a_scaleY;
			return a_wOut > 0.0 && a_hOut > 0.0;
		}

		// does the widget's class (or a base of it) draw something itself: images, text, progress bars, borders
		bool DrawsItself(UE::UClass* a_cls)
		{
			for (UE::UStruct* s = a_cls; s; s = s->superStruct) {
				const std::string n = ue::NameOf(s);
				if (n == "Image" || n == "TextBlock" || n == "RichTextBlock" || n == "ProgressBar" || n == "Border" || n == "CommonTextBlock" ||
					n == "MultiLineEditableText" || n == "EditableText") return true;
			}
			return false;
		}

		// the union of the rectangles of everything the element draws (visible, not faded out); false = nothing drawn
		struct DrawnWalk { double l = 1e9, t = 1e9, r = -1e9, b = -1e9; int count = 0; double sx = 1, sy = 1; };
		void DrawnVisit(UE::UObject* a_w, int a_depth, DrawnWalk& a_d)
		{
			if (!a_w || a_depth > 24 || ++a_d.count > 400) return;
			const auto vis = Visibility(a_w);
			if (vis == kHidden || vis == kCollapsed || Opacity(a_w) <= 0.02f) return;
			auto* cls = a_w->GetClass();
			if (DrawsItself(cls)) {
				double x, y, w, h;
				if (MeasureRect(a_w, a_d.sx, a_d.sy, x, y, w, h)) {
					a_d.l = std::min(a_d.l, x); a_d.t = std::min(a_d.t, y); a_d.r = std::max(a_d.r, x + w); a_d.b = std::max(a_d.b, y + h);
				}
				return;
			}
			if (UserWidgetClass() && cls->IsChildOf(UserWidgetClass())) {
				DrawnVisit(ObjProp(ObjProp(a_w, "WidgetTree"), "RootWidget"), a_depth + 1, a_d);
			}
			if (PanelClass() && cls->IsChildOf(PanelClass())) {
				auto* slots = ue::At<RawArray>(a_w, Off(cls, "Slots"));
				for (std::int32_t i = 0; slots && slots->data && i < slots->num && i < 256; ++i) {
					DrawnVisit(ObjProp(slots->data[i], "Content"), a_depth + 1, a_d);
				}
			} else if (Off(cls, "Content") >= 0) {   // a content widget (SizeBox, ScaleBox, RetainerBox, InvalidationBox ...)
				DrawnVisit(ObjProp(a_w, "Content"), a_depth + 1, a_d);
			}
			if (auto* displayed = ObjProp(a_w, "DisplayedWidget")) {
				DrawnVisit(displayed, a_depth + 1, a_d);
			}
		}

		bool Measure(UE::UObject* a_w, Tracked& a_t, const elements::Element& a_el)
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
			if (ue::Dying(a_w) || !toView.RunGuarded() || !size.RunGuarded() || !view.RunGuarded() || !dpi.RunGuarded()) {
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
			// the geometry was painted with the offset written last frame - through the transform, or through the slot
			// (moveViaSlot; without this the bound chased its own shift twice a second and the wheel flipped between two spots)
			a_t.baseVX = a_t.vx - a_t.lastX - a_t.slotDx;
			a_t.baseVY = a_t.vy - a_t.lastY - a_t.slotDy;
			a_t.measured = a_t.vw > 0.0 && a_t.vh > 0.0;
			// what it draws, clipped to its box
			DrawnWalk d;
			d.sx = a_t.lastSX;
			d.sy = a_t.lastSY;
			DrawnVisit(a_w, 0, d);
			a_t.drawn = d.r > d.l && d.b > d.t;
			if (a_t.drawn) {
				a_t.dx = std::max(d.l, a_t.vx);
				a_t.dy = std::max(d.t, a_t.vy);
				a_t.dw = std::min(d.r, a_t.vx + a_t.vw) - a_t.dx;
				a_t.dh = std::min(d.b, a_t.vy + a_t.vh) - a_t.dy;
				a_t.drawn = a_t.dw > 0.0 && a_t.dh > 0.0;
				// the transparent margins inside the art (elements::Element::visibleW / visibleH), centred
				const double ix = (1.0 - a_el.visibleW) * 0.5 * a_t.dw, iy = (1.0 - a_el.visibleH) * 0.5 * a_t.dh;
				a_t.dx += ix; a_t.dw -= 2.0 * ix; a_t.dy += iy; a_t.dh -= 2.0 * iy;
				a_t.baseDX = a_t.dx - a_t.lastX - a_t.slotDx;
				a_t.baseDY = a_t.dy - a_t.lastY - a_t.slotDy;
			}
			return a_t.measured;
		}

		// the offset an element gets: its own, plus the group's when it is in the group, plus the offset of what it moves
		// with (chains followed, loops cut)
		std::pair<double, double> Offset(const settings::Values& a_s, std::size_t a_i, int a_depth = 0)
		{
			const auto& all = elements::All();
			const auto& e = a_s.elements[a_i];
			double      x = e.x, y = e.y;
			if (a_depth == 0 && a_s.group.Has(all[a_i].key)) {
				x += a_s.group.x;
				y += a_s.group.y;
			}
			std::string with = e.moveWith;
			if (!with.empty() && a_depth < 8) {
				if (const int j = elements::IndexOf(with); j >= 0 && static_cast<std::size_t>(j) != a_i) {
					const auto [wx, wy] = Offset(a_s, static_cast<std::size_t>(j), a_depth + 1);
					x += wx;
					y += wy;
				}
			}
			return { x, y };
		}

		// ---- "Fill from" (2026-09-29) --------------------------------------------------------------------------------
		// The bars share one material (M_UI_BaseProgressBar); which way a bar fills is a STATIC switch permutation of the
		// game's material instance the bar's StatusBar was given (IsSymmetrical on MIC_UI_ProgressBar_HealthHUD,
		// IsLeftToRight=1 on _Fatigue, IsLeftToRight=0 on _MagickaHUD - read live off their StaticParametersRuntime),
		// and a static switch cannot change on a dynamic instance. So the bar's progress image gets a NEW dynamic instance
		// whose parent is the game's instance with the wanted permutation, and every scalar, vector and texture
		// parameter the bar had (its colours, panner speeds, the live progress) is carried over from the old one. The
		// StatusBar's own SetProgress and animations keep working: they take the image's dynamic material each time.
		UE::UObject* PermutationMic(int a_fill)
		{
			static const wchar_t* kPaths[4]{ nullptr,
				L"/Game/UI/Materials/Instances/MIC_UI_ProgressBar_Fatigue.MIC_UI_ProgressBar_Fatigue",       // 1 left
				L"/Game/UI/Materials/Instances/MIC_UI_ProgressBar_HealthHUD.MIC_UI_ProgressBar_HealthHUD",   // 2 centre
				L"/Game/UI/Materials/Instances/MIC_UI_ProgressBar_MagickaHUD.MIC_UI_ProgressBar_MagickaHUD" };   // 3 right
			if (a_fill < 1 || a_fill > 3) return nullptr;
			return UE::StaticFindObject<UE::UObject>(nullptr, nullptr, kPaths[a_fill]);
		}

		UE::UObject* DynamicMaterial(UE::UObject* a_image)
		{
			ue::Call c(a_image, L"GetDynamicMaterial");
			if (!c || !c.Run()) return nullptr;
			auto** r = static_cast<UE::UObject**>(c.At("ReturnValue"));
			return r ? *r : nullptr;
		}

		bool IsBarMaterial(UE::UObject* a_mi)
		{
			auto* parent = a_mi ? ObjProp(a_mi, "Parent") : nullptr;
			return parent && ue::NameOf(parent).starts_with("MIC_UI_ProgressBar_");
		}

		// the first Image under the element whose brush material derives from a progress-bar instance
		UE::UObject* FindBarImage(UE::UObject* a_w, int a_depth = 0)
		{
			if (!a_w || a_depth > 12) return nullptr;
			auto* cls = a_w->GetClass();
			if (ue::NameOf(cls) == "Image") {
				return IsBarMaterial(DynamicMaterial(a_w)) ? a_w : nullptr;
			}
			if (UserWidgetClass() && cls->IsChildOf(UserWidgetClass())) {
				if (auto* f = FindBarImage(ObjProp(ObjProp(a_w, "WidgetTree"), "RootWidget"), a_depth + 1)) return f;
			}
			if (PanelClass() && cls->IsChildOf(PanelClass())) {
				auto* slots = ue::At<RawArray>(a_w, Off(cls, "Slots"));
				for (std::int32_t i = 0; slots && slots->data && i < slots->num && i < 64; ++i) {
					if (auto* f = FindBarImage(ObjProp(slots->data[i], "Content"), a_depth + 1)) return f;
				}
			} else if (Off(cls, "Content") >= 0) {
				if (auto* f = FindBarImage(ObjProp(a_w, "Content"), a_depth + 1)) return f;
			}
			return nullptr;
		}

		// the parameter names an instance overrides (FScalarParameterValue 36 bytes, FVectorParameterValue 48,
		// FTextureParameterValue 40 - each starts with FMaterialParameterInfo whose first field is the FName)
		void ParameterNames(UE::UObject* a_mi, const char* a_prop, std::size_t a_stride, std::vector<std::uint64_t>& a_out)
		{
			if (!a_mi) return;
			const auto off = Off(a_mi->GetClass(), a_prop);
			if (off < 0) return;
			struct Arr { std::uint8_t* data; std::int32_t num, max; };
			const auto* a = ue::At<Arr>(a_mi, off);
			for (std::int32_t i = 0; a && a->data && i < a->num && i < 64; ++i) {
				std::uint64_t n = 0;
				std::memcpy(&n, a->data + static_cast<std::size_t>(i) * a_stride, sizeof(n));
				if (std::ranges::find(a_out, n) == a_out.end()) a_out.push_back(n);
			}
		}

		void ApplyFill(UE::UObject* a_w, Tracked& a_t, const elements::Element& a_el, int a_want)
		{
			const ULONGLONG now = GetTickCount64();
			if (a_want == a_t.fillApplied && now - a_t.fillCheckedAt < 500) return;
			a_t.fillCheckedAt = now;
			if (!a_t.fillImageLooked) {
				a_t.fillImageLooked = true;
				a_t.fillImage = FindBarImage(a_w);
				if (!a_t.fillImage) logger::info("hud: {} has no progress-bar image - \"Fill from\" does not apply", a_el.key);
			}
			if (!a_t.fillImage) return;
			auto* mid = DynamicMaterial(a_t.fillImage);
			if (!mid) return;
			auto* parent = ObjProp(mid, "Parent");
			if (!a_t.fillOriginal && IsBarMaterial(mid)) a_t.fillOriginal = parent;   // the game's own, seen first
			if (!a_t.fillOriginal) return;
			auto* target = a_want == 0 ? a_t.fillOriginal : PermutationMic(a_want);
			if (!target) {
				if (a_want != a_t.fillApplied) logger::warn("hud: {} - the material instance for fill {} is not loaded", a_el.key, a_want);
				a_t.fillApplied = a_want;
				return;
			}
			if (parent == target) {   // already so (the game's own, or ours from before)
				a_t.fillApplied = a_want;
				a_t.fillMid = mid;
				return;
			}
			// every parameter the old instance resolves (its own, its parent's and the target's overrides), read BEFORE the swap
			std::vector<std::uint64_t> scalars, vectors, textures;
			for (auto* mi : { mid, parent, target }) {
				ParameterNames(mi, "ScalarParameterValues", 36, scalars);
				ParameterNames(mi, "VectorParameterValues", 48, vectors);
				ParameterNames(mi, "TextureParameterValues", 40, textures);
			}
			std::vector<std::pair<std::uint64_t, float>>                  sv;
			std::vector<std::pair<std::uint64_t, std::array<float, 4>>>   vv;
			std::vector<std::pair<std::uint64_t, UE::UObject*>>           tv;
			for (const auto n : scalars) {
				ue::Call get(mid, L"K2_GetScalarParameterValue");
				if (!get || !get.At("ParameterName")) break;
				std::memcpy(get.At("ParameterName"), &n, 8);
				get.Run();
				if (const auto* v = static_cast<const float*>(get.At("ReturnValue"))) sv.emplace_back(n, *v);
			}
			for (const auto n : vectors) {
				ue::Call get(mid, L"K2_GetVectorParameterValue");
				if (!get || !get.At("ParameterName")) break;
				std::memcpy(get.At("ParameterName"), &n, 8);
				get.Run();
				if (const auto* v = static_cast<const float*>(get.At("ReturnValue"))) vv.emplace_back(n, std::array<float, 4>{ v[0], v[1], v[2], v[3] });
			}
			for (const auto n : textures) {
				ue::Call get(mid, L"K2_GetTextureParameterValue");
				if (!get || !get.At("ParameterName")) break;
				std::memcpy(get.At("ParameterName"), &n, 8);
				get.Run();
				if (auto** v = static_cast<UE::UObject**>(get.At("ReturnValue")); v && *v) tv.emplace_back(n, *v);
			}
			ue::Call brush(a_t.fillImage, L"SetBrushFromMaterial");
			if (!brush || !brush.At("Material")) return;
			brush.Set("Material", target);
			brush.Run();
			auto* neu = DynamicMaterial(a_t.fillImage);
			if (!neu) return;
			int copied = 0;
			for (const auto& [n, v] : sv) {
				ue::Call set(neu, L"SetScalarParameterValue");
				if (!set || !set.At("ParameterName")) break;
				std::memcpy(set.At("ParameterName"), &n, 8);
				set.Set<float>("Value", v);
				set.Run();
				++copied;
			}
			for (const auto& [n, v] : vv) {
				ue::Call set(neu, L"SetVectorParameterValue");
				if (!set || !set.At("ParameterName") || !set.At("Value")) break;
				std::memcpy(set.At("ParameterName"), &n, 8);
				std::memcpy(set.At("Value"), v.data(), 16);
				set.Run();
				++copied;
			}
			for (const auto& [n, v] : tv) {
				ue::Call set(neu, L"SetTextureParameterValue");
				if (!set || !set.At("ParameterName")) break;
				std::memcpy(set.At("ParameterName"), &n, 8);
				set.Set("Value", v);
				set.Run();
				++copied;
			}
			a_t.fillApplied = a_want;
			a_t.fillMid = neu;
			static const char* kNames[4]{ "the game's own", "the left", "the centre", "the right" };
			logger::info("hud: {} fills from {} - its material re-parented to {} with {} parameter(s) carried over", a_el.key, kNames[std::clamp(a_want, 0, 3)], ue::NameOf(target), copied);
		}

		// ---- "Length follows the resource" (2026-09-29) --------------------------------------------------------------
		// The player's maximum of a resource, from the game's own numbers: the HUD's view model (VHUDMainViewModel) has
		// MaxMagickaValue and the three bars' fractions; the current value comes off the player (Actor::GetActorFloatValue,
		// Oblivion's indices 8 health, 9 magicka, 10 fatigue) and max = current / fraction. The indices are PROVEN before
		// use: magicka's current / MaxMagickaValue must equal the view model's MagickaBarValue.
		struct StatSource
		{
			ue::Handle vm;
			ULONGLONG  scanAt = 0, provenAt = 0, readAt = 0;
			int        proven = 0;   // 0 unknown, 1 yes, -1 no
			double     max[3]{ 0.0, 0.0, 0.0 };   // health, magicka, fatigue
		} g_stat;

		double LinkedLength(const char* a_key, const settings::Element& a_e, bool a_gameplay)
		{
			const int which = std::string_view(a_key) == "Health" ? 0 : std::string_view(a_key) == "Magicka" ? 1 : 2;
			const ULONGLONG now = GetTickCount64();
			auto* vm = g_stat.vm.Get();
			if (!vm && now - g_stat.scanAt >= 5000) {
				g_stat.scanAt = now;
				vm = ue::FirstOf(ue::Class(L"/Script/Altar.VHUDMainViewModel"));
				g_stat.vm.Set(vm);
			}
			// only a PLACED player in gameplay: at the main menu the singleton exists with no character loaded, and its actor
			// values dereference a null inside the game (the crash of 2026-09-29 23:08:54, TestBench crash record)
			auto* player = RE::PlayerCharacter::GetSingleton();
			const bool placed = player && a_gameplay && player->parentCell != nullptr;
			if (vm && placed && g_stat.proven >= 0 && now - g_stat.readAt >= 1000) {
				g_stat.readAt = now;
				auto* cls = vm->GetClass();
				const float* healthBar = ue::At<float>(vm, Off(cls, "HealthBarValue"));
				const float* magickaBar = ue::At<float>(vm, Off(cls, "MagickaBarValue"));
				const float* fatigueBar = ue::At<float>(vm, Off(cls, "FatigueBarValue"));
				const float* maxMagicka = ue::At<float>(vm, Off(cls, "MaxMagickaValue"));
				if (healthBar && magickaBar && fatigueBar && maxMagicka) {
					const double h = player->GetActorFloatValue(static_cast<RE::ActorValue::Index>(8));
					const double m = player->GetActorFloatValue(static_cast<RE::ActorValue::Index>(9));
					const double f = player->GetActorFloatValue(static_cast<RE::ActorValue::Index>(10));
					if (g_stat.proven == 0 && *maxMagicka > 0.0f && *magickaBar > 0.05f && now - g_stat.provenAt >= 2000) {
						g_stat.provenAt = now;
						const double ratio = m / *maxMagicka;
						g_stat.proven = std::abs(ratio - *magickaBar) < 0.03 ? 1 : -1;
						if (g_stat.proven > 0) logger::info("hud: the actor value indices are proven (magicka {:.0f} of {:.0f} = the bar's {:.3f})", m, *maxMagicka, *magickaBar);
						else logger::warn("hud: the actor value indices are NOT Oblivion's (magicka read {:.1f}, the HUD says {:.3f} of {:.0f}) - \"Length follows the resource\" is off", m, *magickaBar, *maxMagicka);
					}
					if (g_stat.proven > 0) {
						if (*healthBar > 0.05f && h > 0.0) g_stat.max[0] = h / *healthBar;
						if (*maxMagicka > 0.0f) g_stat.max[1] = *maxMagicka;
						if (*fatigueBar > 0.05f && f > 0.0) g_stat.max[2] = f / *fatigueBar;
					}
				}
			}
			if (g_stat.proven <= 0 || g_stat.max[which] <= 0.0 || a_e.pointsPerLength <= 0.0f) return 1.0;
			return std::clamp(g_stat.max[which] / static_cast<double>(a_e.pointsPerLength), 0.25, 4.0);
		}

		// Minimap Menu (another of the owner's plugins) moves the location banner to sit by its map; while its export says so,
		// this mod leaves the Location element's transform alone. The export is looked up once the DLL is loaded, at most
		// every 5 s until then.
		// a Minimap Menu export by name (MinimapMenu_OwnsLocationPopup, MinimapMenu_OwnsCompass): looked up once the DLL is
		// loaded, at most every 5 s until then; false while the DLL or the export is not there
		struct MinimapExport
		{
			const char* name;
			bool (*fn)() = nullptr;
			ULONGLONG askedAt = 0;
			bool      logged = false;
			bool Ask()
			{
				if (!fn && GetTickCount64() - askedAt >= 5000) {
					askedAt = GetTickCount64();
					if (HMODULE m = ::GetModuleHandleW(L"MinimapMenu.dll")) {
						fn = reinterpret_cast<bool (*)()>(::GetProcAddress(m, name));
						if (!logged) { logged = true; logger::info("hud: Minimap Menu is loaded{} {}", fn ? " - it answers" : ", without", name); }
					}
				}
				return fn && fn();
			}
		};
		bool MinimapOwnsLocation() { static MinimapExport e{ "MinimapMenu_OwnsLocationPopup" }; return e.Ask(); }
		bool MinimapOwnsCompass() { static MinimapExport e{ "MinimapMenu_OwnsCompass" }; return e.Ask(); }

		// Length / Height as a layout size (elements::Element::sizeImage): the named image's desired size overridden to its
		// own size times the two factors, so its row grows one way (a HorizontalBox lays the rest out after it) and the
		// texts beside it keep their size; a second image (the bar's highlight) follows the height alone. Back to the
		// image's own size at 1.00 / 1.00.
		void ApplyLayoutSize(UE::UObject* a_w, Tracked& a_t, const elements::Element& a_el, float a_x, float a_y)
		{
			auto* img = ObjProp(a_w, a_el.sizeImage);
			if (!img) return;
			if (a_t.sizeBaseW <= 0.0) {   // the image's own size, from its brush
				static auto* brushStruct = UE::StaticFindObject<UE::UStruct>(nullptr, nullptr, L"/Script/SlateCore.SlateBrush");
				const auto off = Off(img->GetClass(), "Brush");
				const auto sz = brushStruct ? Off(brushStruct, "ImageSize") : -1;
				const float* v = off >= 0 && sz >= 0 ? ue::At<float>(img, off + sz) : nullptr;
				if (!v || v[0] <= 0.0f || v[1] <= 0.0f) return;
				a_t.sizeBaseW = v[0];
				a_t.sizeBaseH = v[1];
			}
			const double wantX = a_t.sizeBaseW * a_x, wantY = a_t.sizeBaseH * a_y;
			if (std::abs(wantX - a_t.sizeAppliedX) < 0.01 && std::abs(wantY - a_t.sizeAppliedY) < 0.01) return;
			const auto set = [](UE::UObject* a_img, double a_wx, double a_wy) {
				ue::Call c(a_img, L"SetDesiredSizeOverride");
				const double v[2] = { a_wx, a_wy };
				void* p = c.At("DesiredSize");   // UImage::SetDesiredSizeOverride(FVector2D DesiredSize), probed 2026-09-29
				if (!p) p = c.At("DesiredSizeOverride");
				if (p) { std::memcpy(p, v, sizeof(v)); c.Run(); }
			};
			set(img, wantX, wantY);
			if (a_el.sizeImage2) {
				if (auto* img2 = ObjProp(a_w, a_el.sizeImage2)) {
					static auto* brushStruct = UE::StaticFindObject<UE::UStruct>(nullptr, nullptr, L"/Script/SlateCore.SlateBrush");
					const auto off = Off(img2->GetClass(), "Brush");
					const auto sz = brushStruct ? Off(brushStruct, "ImageSize") : -1;
					const float* v = off >= 0 && sz >= 0 ? ue::At<float>(img2, off + sz) : nullptr;
					if (v && v[0] > 0.0f) set(img2, v[0], a_t.sizeBaseH * a_y);
				}
			}
			a_t.sizeAppliedX = wantX;
			a_t.sizeAppliedY = wantY;
			logger::debug("hud: {} - its {} sized {:.0f} x {:.0f}", a_el.key, a_el.sizeImage, wantX, wantY);
		}

		// ---- the two indicators this mod BUILDS (2026-09-30) --------------------------------------------------------
		// The engine's object constructor, fault-guarded: a construction that faults returns nothing instead of taking the
		// game down (the parameters live in the caller, so this frame has nothing to unwind).
		UE::UObject* ConstructGuarded(const UE::FStaticConstructObjectParameters& a_p)
		{
			__try {
				return UE::StaticConstructObject_Internal(a_p);
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return nullptr;
			}
		}

		UE::UObject* NewWidget(const wchar_t* a_class, UE::UObject* a_outer)
		{
			auto* cls = ue::Class(a_class);
			if (!cls || !a_outer) return nullptr;
			UE::FStaticConstructObjectParameters p(cls);
			p.outer = a_outer;
			return ConstructGuarded(p);
		}

		// the HUD's own layer (WBP_ModernHud_PrimaryLayout), through the Health widget's outers
		UE::UObject* HudLayer()
		{
			const int h = elements::IndexOf("Health");
			for (UE::UObject* o = h >= 0 ? g_el[static_cast<std::size_t>(h)].widget.Get() : nullptr; o; o = o->GetOuter()) {
				if (ue::NameOf(o->GetClass()) == "WBP_ModernHud_PrimaryLayout_C") return o;
			}
			return nullptr;
		}

		// a child added to a panel, its slot aligned (EHorizontalAlignment / EVerticalAlignment: 2 = centre)
		UE::UObject* AddAligned(UE::UObject* a_panel, UE::UObject* a_child, std::uint8_t a_h, std::uint8_t a_v)
		{
			ue::Call add(a_panel, L"AddChild");
			if (!add || !add.At("Content")) return nullptr;
			add.Set("Content", a_child);
			add.Run();
			auto** sp = static_cast<UE::UObject**>(add.At("ReturnValue"));
			auto* slot = sp ? *sp : nullptr;
			if (slot) {
				for (const auto& [fn, arg, v] : std::initializer_list<std::tuple<const wchar_t*, const char*, std::uint8_t>>{
						 { L"SetHorizontalAlignment", "InHorizontalAlignment", a_h }, { L"SetVerticalAlignment", "InVerticalAlignment", a_v } }) {
					ue::Call set(slot, fn);
					if (void* p = set.At(arg)) { *static_cast<std::uint8_t*>(p) = v; set.Run(); }
				}
			}
			return slot;
		}

		void SetImageSize(UE::UObject* a_img, double a_w, double a_h) { Call2(a_img, L"SetDesiredSizeOverride", "DesiredSize", a_w, a_h); }

		void SetTint(UE::UObject* a_img, float a_r, float a_g, float a_b)
		{
			ue::Call c(a_img, L"SetColorAndOpacity");
			const float v[4] = { a_r, a_g, a_b, 1.0f };
			if (void* p = c.At("InColorAndOpacity")) { std::memcpy(p, v, sizeof(v)); c.Run(); }
		}

		void SetAngle(UE::UObject* a_w, float a_deg)
		{
			ue::Call c(a_w, L"SetRenderTransformAngle");
			if (c && c.At("Angle")) { c.Set<float>("Angle", a_deg); c.Run(); }
		}

		UE::UObject* CreateIndicator(const elements::Element& a_el, Tracked& a_t)
		{
			auto* hud = HudLayer();
			auto* tree = ObjProp(hud, "WidgetTree");
			auto* root = ObjProp(tree, "RootWidget");
			if (!tree || !root) return nullptr;   // not yet: Find() asks again every 2 s
			auto* box = NewWidget(L"/Script/UMG.Overlay", tree);
			if (!box) { logger::warn("hud: {} - the engine made no Overlay; the indicator is not built", a_el.key); return nullptr; }
			if (!AddAligned(root, box, 2, 2)) return nullptr;
			const bool damage = std::string_view(a_el.createNative) == "damage";
			// a transparent image gives the box its size (the ring's), so it can be placed and measured like any element
			if (auto* area = NewWidget(L"/Script/UMG.Image", tree)) {
				AddAligned(box, area, 2, 2);
				SetImageSize(area, damage ? 340.0 : 260.0, damage ? 340.0 : 260.0);
				SetOpacity(area, 0.0f);
				a_t.area = area;
			}
			for (int k = 0; k < (damage ? 8 : 12); ++k) {
				auto* m = NewWidget(L"/Script/UMG.Image", tree);
				if (!m) break;
				AddAligned(box, m, 2, 2);
				if (damage) SetTint(m, 0.95f, 0.06f, 0.03f);
				else SetTint(m, 1.0f, 1.0f, 1.0f);
				SetOpacity(m, 0.0f);
				a_t.marks.push_back(m);
			}
			a_t.markOp.assign(a_t.marks.size(), 0.0f);
			a_t.markAng.assign(a_t.marks.size(), -999.0f);
			a_t.markColour.assign(a_t.marks.size(), damage ? 1.0f : 0.0f);
			a_t.markSize.assign(a_t.marks.size(), 20.0f);
			SetVisibility(box, kSelfHitTestInvisible);
			a_t.created = true;
			logger::info("hud: {} built on the HUD layer ({} marks)", a_el.key, a_t.marks.size());
			return box;
		}

		// the game's HUD numbers the indicators read, once per frame at most
		struct IndicatorData
		{
			bool                               ok = false;
			float                              health = -1.0f, heading = 0.0f, detection = 0.0f;
			bool                               sneaking = false;
			std::vector<std::pair<float, float>> hostiles;   // (distance cm, bearing deg)
		};

		// ---- who can detect the player (2026-09-30) ----------------------------------------------------------------------
		// Oblivion's HighProcess keeps, at +0x2B8, a BSSimpleList of detection entries - the actors that process's actor
		// detects: { Actor* actor; +0x08 u8 level (0 lost, 1 unseen, 2 noticed, 3 seen); +0x10 s32 value }. The PLAYER's
		// list names the actors around them; each of those actors' own list holds its entry for the player, which is how
		// well that actor detects the player. Probed live on 2026-09-30 (the prison guards moving 2 <-> 3 as they looked).
		// A process is read only when its vtable is the player's own (HighProcess) - lower process levels have no such list.
		struct ObserverRaw
		{
			float        bearing;   // clockwise from north, degrees (Oblivion's heading convention, as the compass)
			float        distance;  // game units
			std::int32_t level;
			std::int32_t value;
		};

		constexpr std::ptrdiff_t kProcessOffset = 0x138;    // MobileObject::currentProcess
		constexpr std::ptrdiff_t kDetectionList = 0x2B8;    // HighProcess: the detection entries
		constexpr std::ptrdiff_t kLocation = 0x64;          // TESObjectREFR::data.location

		// fault-guarded (no C++ objects in this frame): the count written, or -1 when a read faulted
		int ReadObserversRaw(ObserverRaw* a_out, int a_cap, float a_maxDistance)
		{
			__try {
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player || !player->parentCell) return 0;
				const auto base = reinterpret_cast<std::uintptr_t>(player);
				const auto proc = *reinterpret_cast<const std::uintptr_t*>(base + kProcessOffset);
				if (!proc) return 0;
				const auto highVt = *reinterpret_cast<const std::uintptr_t*>(proc);
				struct Node { std::uintptr_t item; const Node* next; };
				const float* pl = reinterpret_cast<const float*>(base + kLocation);
				int n = 0, steps = 0;
				for (auto* node = *reinterpret_cast<const Node* const*>(proc + kDetectionList); node && steps < 96 && n < a_cap; node = node->next, ++steps) {
					if (!node->item) continue;
					const auto actor = *reinterpret_cast<const std::uintptr_t*>(node->item);
					if (!actor || actor == base) continue;
					const auto ap = *reinterpret_cast<const std::uintptr_t*>(actor + kProcessOffset);
					if (!ap || *reinterpret_cast<const std::uintptr_t*>(ap) != highVt) continue;
					const float* al = reinterpret_cast<const float*>(actor + kLocation);
					const float dx = al[0] - pl[0], dy = al[1] - pl[1];
					const float dist = std::sqrt(dx * dx + dy * dy);
					if (!(dist <= a_maxDistance)) continue;
					int s2 = 0;
					for (auto* e = *reinterpret_cast<const Node* const*>(ap + kDetectionList); e && s2 < 96; e = e->next, ++s2) {
						if (e->item && *reinterpret_cast<const std::uintptr_t*>(e->item) == base) {
							a_out[n].bearing = static_cast<float>(std::atan2(dx, dy) * 180.0 / std::numbers::pi);
							a_out[n].distance = dist;
							a_out[n].level = *reinterpret_cast<const std::uint8_t*>(e->item + 0x08);
							a_out[n].value = *reinterpret_cast<const std::int32_t*>(e->item + 0x10);
							++n;
							break;
						}
					}
				}
				return n;
			} __except (EXCEPTION_EXECUTE_HANDLER) {
				return -1;
			}
		}

		// the observers, read at most four times a second (hud-mods-limit-per-frame-work): nearest first
		const std::vector<ObserverRaw>& Observers()
		{
			static std::vector<ObserverRaw> cache;
			static ULONGLONG                at = 0, loggedAt = 0;
			static bool                     faultLogged = false;
			const ULONGLONG                 now = GetTickCount64();
			if (now - at < 250) return cache;
			at = now;
			std::array<ObserverRaw, 32> buf{};
			const int n = ReadObserversRaw(buf.data(), static_cast<int>(buf.size()), 4200.0f);   // ~60 m
			if (n < 0) {
				if (!faultLogged) logger::warn("sneak indicator: reading the detection lists faulted; no marks until it reads cleanly");
				faultLogged = true;
				cache.clear();
				return cache;
			}
			cache.assign(buf.begin(), buf.begin() + n);
			std::ranges::sort(cache, {}, &ObserverRaw::distance);
			if (now - loggedAt >= 5000 && !cache.empty()) {   // the numbers behind the marks, every 5 s while there are any
				loggedAt = now;
				std::string s;
				for (std::size_t i = 0; i < cache.size() && i < 8; ++i) s += std::format(" [bearing {:.0f}, {:.0f} units, level {}, value {}]", cache[i].bearing, cache[i].distance, cache[i].level, cache[i].value);
				logger::debug("sneak indicator: {} actor(s) with an entry for the player{}", cache.size(), s);
			}
			return cache;
		}

		// the marks' art (2026-09-30): arcs drawn for this mod (tools/gen-indicator-art.py) - white ring segments whose
		// ring radius in the texture is kArtRingRadius pixels, so a mark sized radius * W / kArtRingRadius lies on the ring
		constexpr float kArtRingRadius = 420.0f;
		struct ArcArt { const wchar_t* file; float w, h; };
		constexpr ArcArt kDamageArt{ L"DamageArc.png", 480.0f, 150.0f };
		constexpr ArcArt kSneakArt{ L"SneakArc.png", 220.0f, 70.0f };

		// a PNG beside the plugin as a texture, through the engine's own importer (KismetRenderingLibrary)
		UE::UObject* ImportArt(const std::filesystem::path& a_file, UE::UObject* a_context)
		{
			static UE::UObject* lib = nullptr;
			if (!lib) {
				auto* cls = UE::StaticFindObject<UE::UClass>(nullptr, nullptr, L"/Script/Engine.KismetRenderingLibrary");
				lib = cls ? cls->GetDefaultObject(false) : nullptr;
			}
			std::error_code ec;
			if (!lib || !std::filesystem::exists(a_file, ec)) return nullptr;
			ue::Call c(lib, L"ImportFileAsTexture2D");
			void* ctx = c ? c.At("WorldContextObject") : nullptr;
			void* name = c ? c.At("Filename") : nullptr;
			void* ret = c ? c.At("ReturnValue") : nullptr;
			if (!ctx || !name || !ret || ue::Dying(a_context)) return nullptr;
			*static_cast<UE::UObject**>(ctx) = a_context;
			new (name) UE::FString(a_file.wstring().c_str());
			if (!c.RunGuarded()) return nullptr;
			return *static_cast<UE::UObject**>(ret);
		}

		// the arcs on the marks: this mod's own, else the game's soft glow line; asked again every 2 s until one is there
		void SetMarkArt(Tracked& a_t, bool a_damage)
		{
			if (a_t.artSet || GetTickCount64() - a_t.artTriedAt < 2000 || a_t.marks.empty()) return;
			a_t.artTriedAt = GetTickCount64();
			static ue::Handle damageTex, sneakTex;   // imported once; kept alive by the brushes that hold them
			auto& held = a_damage ? damageTex : sneakTex;
			const auto file = settings::PluginFolder() / L"HUDPositionManager" / L"Art" / (a_damage ? kDamageArt.file : kSneakArt.file);
			auto* tex = held.Get();
			bool own = tex != nullptr;
			if (!tex) {
				tex = ImportArt(file, a_t.marks.front());
				own = tex != nullptr;
				if (tex) held.Set(tex);
			}
			if (!tex) {
				logger::warn("hud: {} could not be imported as a texture; the {} marks use the game's T_Line_glow", file.string(), a_damage ? "damage" : "sneak");
				tex = UE::StaticFindObject<UE::UObject>(nullptr, nullptr, L"/Game/Art/UI/Common/T_Line_glow.T_Line_glow");
			}
			if (!tex) return;   // neither yet: asked again in 2 s; the marks stay plain until then
			for (std::size_t k = 0; k < a_t.marks.size(); ++k) {
				auto* m = a_t.marks[k];
				if (!ue::IsLive(m)) continue;
				ue::Call c(m, L"SetBrushFromTexture");
				if (!c || !c.At("Texture")) return;
				c.Set("Texture", tex);
				if (void* p = c.At("bMatchSize")) *static_cast<bool*>(p) = false;
				c.Run();
				a_t.markSize[k] = -1.0f;   // sized again from the ring on the next placement
			}
			a_t.artSet = true;
			logger::info("hud: the {} marks wear {}", a_damage ? "damage" : "sneak", own ? file.filename().string() : std::string("the game's T_Line_glow"));
		}

		UE::UObject* LiveOf(ue::Handle& a_h, ULONGLONG& a_scanAt, const wchar_t* a_class)
		{
			auto* o = a_h.Get();
			if (!o && GetTickCount64() - a_scanAt >= 5000) {
				a_scanAt = GetTickCount64();
				o = ue::FirstOf(ue::Class(a_class));
				a_h.Set(o);
			}
			return o;
		}

		IndicatorData ReadIndicatorData()
		{
			static ue::Handle mainVm, reticleVm, vignette;
			static ULONGLONG  mainAt = 0, reticleAt = 0, vignetteAt = 0;
			IndicatorData d;
			auto* vm = LiveOf(mainVm, mainAt, L"/Script/Altar.VHUDMainViewModel");
			if (!vm) return d;
			auto* cls = vm->GetClass();
			if (const float* p = ue::At<float>(vm, Off(cls, "HealthBarValue"))) d.health = *p;
			if (const float* p = ue::At<float>(vm, Off(cls, "CompassDirectionValue"))) d.heading = *p;
			struct Arr { const float* data; std::int32_t num, max; };
			if (const auto* a = ue::At<Arr>(vm, Off(cls, "HostileData")); a && a->data && a->num > 0 && a->num < 256) {
				for (std::int32_t i = 0; i < a->num; ++i) d.hostiles.emplace_back(a->data[i * 2], a->data[i * 2 + 1]);   // FHostileData {Distance, Angle}
			}
			if (auto* rv = LiveOf(reticleVm, reticleAt, L"/Script/Altar.VHUDReticleViewModel")) {
				if (const float* p = ue::At<float>(rv, Off(rv->GetClass(), "SneakDetectionLevel"))) d.detection = std::clamp(*p, 0.0f, 1.0f);
			}
			// sneaking: the HUD vignette's own flag (WBP_Modern_Hud_Vignette on the HUD layer's root)
			auto* vg = vignette.Get();
			if (!vg && GetTickCount64() - vignetteAt >= 5000) {
				vignetteAt = GetTickCount64();
				if (auto* root = ObjProp(ObjProp(HudLayer(), "WidgetTree"), "RootWidget"); root && PanelClass() && root->GetClass()->IsChildOf(PanelClass())) {
					auto* slots = ue::At<RawArray>(root, Off(root->GetClass(), "Slots"));
					for (std::int32_t i = 0; slots && slots->data && i < slots->num && i < 64 && !vg; ++i) {
						auto* c = ObjProp(slots->data[i], "Content");
						if (c && ue::NameOf(c->GetClass()) == "WBP_Modern_Hud_Vignette_C") vg = c;
					}
				}
				vignette.Set(vg);
			}
			if (vg) {
				if (const bool* p = ue::At<bool>(vg, Off(vg->GetClass(), "bSneaking"))) d.sneaking = *p;
			}
			d.ok = true;
			return d;
		}

		float RelativeBearing(float a_bearing, float a_heading)
		{
			float r = std::fmod(a_bearing - a_heading, 360.0f);
			if (r > 180.0f) r -= 360.0f;
			if (r <= -180.0f) r += 360.0f;
			return r;
		}

		// one mark on the ring: its direction (0 = straight ahead, + clockwise), opacity, colour (0 white .. 1 red) and size
		void PlaceMark(Tracked& a_t, std::size_t a_k, bool a_damage, float a_angle, float a_op, float a_colour, float a_size)
		{
			auto* m = a_t.marks[a_k];
			if (!ue::IsLive(m)) return;
			const float radius = a_t.ringRadius > 0.0f ? a_t.ringRadius : (a_damage ? 150.0f : 110.0f);
			if (a_op > 0.0f && std::abs(a_angle - a_t.markAng[a_k]) > 0.2f) {
				const double rad = a_angle * std::numbers::pi / 180.0;
				Call2(m, L"SetRenderTranslation", "Translation", radius * std::sin(rad), -radius * std::cos(rad));
				SetAngle(m, a_angle);   // the arc lies along the ring
				a_t.markAng[a_k] = a_angle;
			}
			if (!a_damage && std::abs(a_colour - a_t.markColour[a_k]) > 0.02f) {
				// white (hidden) -> yellow (half) -> red (detected)
				const float c = std::clamp(a_colour, 0.0f, 1.0f);
				const float r = c < 0.5f ? 1.0f : 1.0f - (c - 0.5f) * 0.2f;
				const float g = c < 0.5f ? 1.0f - c * 0.3f : 0.85f - (c - 0.5f) * 1.6f;
				const float b = c < 0.5f ? 1.0f - c * 1.6f : 0.2f - (c - 0.5f) * 0.3f;
				SetTint(m, r, std::max(g, 0.05f), std::max(b, 0.03f));
				a_t.markColour[a_k] = a_colour;
			}
			if (a_size > 0.0f && std::abs(a_size - a_t.markSize[a_k]) > 0.01f) {   // a_size: a multiple of the ring's own arc
				const ArcArt& art = a_damage ? kDamageArt : kSneakArt;
				const double  f = radius / kArtRingRadius * a_size;
				SetImageSize(m, art.w * f, art.h * f);
				a_t.markSize[a_k] = a_size;
			}
			if (std::abs(a_op - a_t.markOp[a_k]) > 0.01f) {
				SetOpacity(m, a_op);
				a_t.markOp[a_k] = a_op;
			}
		}

		void UpdateIndicator(UE::UObject* a_w, Tracked& a_t, const elements::Element& a_el, bool a_gameplay, bool a_preview, float a_radius)
		{
			(void)a_w;
			if (a_t.marks.empty()) return;
			const bool damage = std::string_view(a_el.createNative) == "damage";
			SetMarkArt(a_t, damage);
			const float radius = a_radius > 0.0f ? a_radius : settings::DefaultRadius(a_el.createNative);
			if (std::abs(radius - a_t.ringRadius) > 0.5f) {   // a new radius: every mark is placed again, and the box sized to the ring
				a_t.ringRadius = radius;
				std::ranges::fill(a_t.markAng, -999.0f);
				std::ranges::fill(a_t.markSize, -1.0f);
				const double arcH = radius / kArtRingRadius * (damage ? kDamageArt.h : kSneakArt.h) * 1.2;
				if (a_t.area && ue::IsLive(a_t.area)) SetImageSize(a_t.area, radius * 2.0 + arcH, radius * 2.0 + arcH);
			}
			const ULONGLONG now = GetTickCount64();
			const IndicatorData d = ReadIndicatorData();
			std::vector<std::tuple<float, float, float, float>> show;   // (angle, opacity, colour 0 white .. 0.5 yellow .. 1 red, size x the ring's arc)
			if (a_preview) {
				if (damage) show = { { -60.0f, 0.85f, 1.0f, 1.0f }, { 5.0f, 0.85f, 1.0f, 1.0f }, { 125.0f, 0.85f, 1.0f, 1.0f } };
				else show = { { -50.0f, 0.8f, 0.0f, 1.0f }, { 30.0f, 0.95f, 0.5f, 1.0f }, { 150.0f, 1.0f, 1.0f, 1.12f } };
			} else if (damage) {
				if (d.ok && a_gameplay && a_t.lastHealth >= 0.0f && d.health >= 0.0f && d.health < a_t.lastHealth - 0.002f) {
					const float drop = a_t.lastHealth - d.health;
					int n = 0;
					std::string seen;
					for (const auto& [dist, bearing] : d.hostiles) {
						if (dist > 3000.0f) continue;
						const float rel = RelativeBearing(bearing, d.heading);
						a_t.flashes.push_back({ rel, now, std::clamp(0.55f + drop * 4.0f, 0.55f, 1.0f) });
						if (seen.size() < 200) seen += std::format(" [bearing {:.0f}, distance {:.0f} cm -> {:.0f}]", bearing, dist, rel);
						++n;
					}
					if (now - a_t.hitLoggedAt >= 1000) {   // the numbers behind the arcs, at most once a second
						a_t.hitLoggedAt = now;
						logger::info("damage indicator: hit (health {:.3f} -> {:.3f}), view bearing {:.0f}, {} hostile(s) within 30 m{}", a_t.lastHealth, d.health, d.heading, n, seen);
					}
					while (a_t.flashes.size() > a_t.marks.size()) a_t.flashes.erase(a_t.flashes.begin());
				}
				if (d.ok && d.health >= 0.0f) a_t.lastHealth = d.health;
				std::erase_if(a_t.flashes, [&](const IndicatorFlash& f) { return now - f.at >= 1500; });
				for (const auto& f : a_t.flashes) {
					const float age = static_cast<float>(now - f.at) / 1500.0f;
					show.emplace_back(f.angle, f.strength * (1.0f - age), 1.0f, 1.0f);
				}
			} else if (d.ok && a_gameplay && d.sneaking) {
				// every actor with an entry for the player, an arc in its direction: white - undetected (unseen); yellow - partly
				// detected (noticed); red and a little larger - detected (seen). Lost (0) is not drawn.
				for (const auto& o : Observers()) {
					if (o.level <= 0 || show.size() >= a_t.marks.size()) continue;
					const float rel = RelativeBearing(o.bearing, d.heading);
					if (o.level >= 3) show.emplace_back(rel, 1.0f, 1.0f, 1.12f);
					else if (o.level == 2) show.emplace_back(rel, 0.95f, 0.5f, 1.0f);
					else show.emplace_back(rel, 0.8f, 0.0f, 1.0f);
				}
			}
			for (std::size_t k = 0; k < a_t.marks.size(); ++k) {
				if (k < show.size()) {
					const auto& [ang, op, col, size] = show[k];
					PlaceMark(a_t, k, damage, ang, op, col, size);
				} else {
					PlaceMark(a_t, k, damage, a_t.markAng[k], 0.0f, a_t.markColour[k], a_t.markSize[k]);
				}
			}
		}

		// The wait / sleep menu. Oblivion Remastered keeps menuMode at 1 (gameplay) through it and through the countdown of a
		// wait, so "always visible" held the Level gauge on screen while the game's own HUD had gone (the owner, 2026-10-01:
		// "the current level meter ... shows during wait time, which shouldn't happen"). Up when the menu's view model says
		// bVisible AND its widget (WBP_ModernMenu_SleepWait_C) is live, in a parent, not hidden and not faded out - the view
		// model alone read bVisible true at an idle capture. Read four times a second; the widget is searched for only while
		// the view model says it is up, at most every second.
		bool WaitMenuUp()
		{
			static ue::Handle vm, menu;
			static ULONGLONG  readAt = 0, vmScanAt = 0, menuScanAt = 0;
			static bool       up = false;
			const ULONGLONG   nowMs = GetTickCount64();
			if (nowMs - readAt < 250) return up;
			readAt = nowMs;
			auto* v = vm.Get();
			if (!v && nowMs - vmScanAt >= 5000) {
				vmScanAt = nowMs;
				v = ue::FirstOf(ue::Class(L"/Script/Altar.VSleepWaitMenuViewModel"));
				vm.Set(v);
			}
			const bool* vmVisible = v ? ue::At<bool>(v, Off(v->GetClass(), "bVisible")) : nullptr;
			bool now = false;
			if (vmVisible && *vmVisible) {
				auto* m = menu.Get();
				if ((!m || !ObjProp(m, "Slot")) && nowMs - menuScanAt >= 1000) {
					menuScanAt = nowMs;
					m = nullptr;
					if (auto* cls = ClassByName("WBP_ModernMenu_SleepWait_C")) {
						auto* arr = UE::FUObjectArray::GetSingleton();
						arr->LockInternalArray();
						const std::int32_t n = arr->GetObjectArrayNum();
						for (std::int32_t i = 0; i < n && !m; ++i) {
							auto* item = arr->IndexToObject(i);
							auto* o = item ? reinterpret_cast<UE::UObject*>(item->object) : nullptr;
							// a template (class default or archetype, flags 0x30) is never the menu on screen
							if (o && o->GetClass() == cls && (static_cast<std::int32_t>(o->objectFlags) & 0x30) == 0 && ObjProp(o, "Slot")) m = o;
						}
						arr->UnlockInternalArray();
					}
					menu.Set(m);
				}
				now = m && ObjProp(m, "Slot") && Visibility(m) != kHidden && Visibility(m) != kCollapsed && Opacity(m) > 0.05f;
			}
			if (now != up) logger::info("hud: the wait menu is {} - this mod's own elements {} and nothing is held visible", now ? "up" : "gone", now ? "hide" : "come back");
			up = now;
			return up;
		}

		void Apply(const settings::Values& a_s, bool a_gameplay, bool a_hideOurs)
		{
			const auto& all = elements::All();
			const std::vector<Rect> rects = (a_s.noOverlap || a_s.snapEdges) ? TrackedRects() : std::vector<Rect>(g_el.size());   // the neighbours' art, as measured
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
				if (t.created && GetTickCount64() - t.levelCheckedAt >= 1000) {   // this mod's own level gauge follows the player
					t.levelCheckedAt = GetTickCount64();
					SetLevelGauge(w, t);
				}
				if (t.created && !t.held) {
					// Always visible off: it shows and fades with the bars - the Health bar's image opacity, copied when it changes
					float want = 1.0f;
					if (const int h = elements::IndexOf("Health"); h >= 0) {
						auto& ht = g_el[static_cast<std::size_t>(h)];
						if (ht.fillImage && ue::IsLive(ht.fillImage)) want = Opacity(ht.fillImage);
					}
					if (std::abs(want - t.followedOpacity) > 0.01f) {
						SetOpacity(w, want);
						t.followedOpacity = want;
					}
				} else if (t.created) {
					t.followedOpacity = -1.0f;   // held at 1 by the hold; followed again when it ends
				}
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
				const bool minimapOwns = (std::string_view(all[i].key) == "Location" && MinimapOwnsLocation()) ||
				                         (std::string_view(all[i].key) == "Compass" && MinimapOwnsCompass());
				auto [ox, oy] = a_s.enabled && !minimapOwns ? Offset(a_s, i) : std::pair<double, double>{ 0.0, 0.0 };
				// the settings hold percent of the screen; the render transform takes layout units (1920 x 1080 at every
				// DPI, wider on a wide screen - the measured viewport, or the designer's size until it is measured)
				const double unitW = g_viewW > 0.0 ? g_viewW : 1920.0, unitH = g_viewH > 0.0 ? g_viewH : 1080.0;
				ox = ox / 100.0 * unitW;
				oy = oy / 100.0 * unitH;
				const double scale = a_s.enabled && !minimapOwns ? e.scale : 1.0;
				const double linked = a_s.enabled && e.linkLength && all[i].stat ? LinkedLength(all[i].key, e, a_gameplay) : 1.0;   // "Length follows the resource"
				const bool   sizeByLayout = all[i].sizeImage != nullptr;   // Length / Height as the named image's layout size, not the render scale
				const double scaleX = scale * (a_s.enabled && !minimapOwns && !sizeByLayout ? e.stretchX * linked : 1.0), scaleY = scale * (a_s.enabled && !minimapOwns && !sizeByLayout ? e.stretchY : 1.0);   // Length / Height on top of Size
				if (sizeByLayout) ApplyLayoutSize(w, t, all[i], a_s.enabled ? e.stretchX : 1.0f, a_s.enabled ? e.stretchY : 1.0f);
				if (all[i].bar) ApplyFill(w, t, all[i], a_s.enabled ? e.fill : 0);
				// the rectangle on screen, twice a second; the offset is clamped so the element never leaves the screen
				const ULONGLONG nowMs = GetTickCount64();
				if (nowMs - t.measuredAt >= 500) {
					t.measuredAt = nowMs;
					Measure(w, t, all[i]);
				}
				if (t.measured && a_s.enabled) {
					// against the anchor fixed at measurement, never against a value this frame changes
					if (t.drawn) {
						// the ART reaches the screen's edges (the compass strip sits 21 units below its box's top, 2026-09-29)
						ox = std::clamp(ox, -t.baseDX, std::max(-t.baseDX, g_viewW - t.baseDX - t.dw));
						oy = std::clamp(oy, -t.baseDY, std::max(-t.baseDY, g_viewH - t.baseDY - t.dh));
					} else {
						const double insetX = (1.0 - all[i].visibleW) * 0.5 * t.vw;   // the undrawn margin may leave the screen
						ox = std::clamp(ox, -(t.baseVX + insetX), std::max(-(t.baseVX + insetX), g_viewW - t.baseVX - t.vw + insetX));
						oy = std::clamp(oy, -t.baseVY, std::max(-t.baseVY, g_viewH - t.baseVY - t.vh));
					}
					if (a_s.noOverlap && t.drawn) {
						// and against the neighbours' DRAWN edges, from where this element's art is now (measured) to where it wants to go
						const Rect me{ true, t.dx, t.dy, t.dx + t.dw, t.dy + t.dh };
						double lMin, lMax, tMin, tMax;
						NeighbourLimits(a_s, i, me, rects, lMin, lMax, tMin, tMax);
						if (lMin <= lMax) ox = std::clamp(ox, lMin - t.baseDX, lMax - t.baseDX);
						if (tMin <= tMax) oy = std::clamp(oy, tMin - t.baseDY, tMax - t.baseDY);
					}
					if (a_s.snapEdges && a_s.snapDistance > 0.0f && t.drawn && !(e.x == 0.0f && e.y == 0.0f && e.scale == 1.0f && e.stretchX == 1.0f && e.stretchY == 1.0f)) {
						// the art's edges where the offset puts them, pulled onto the nearest edge of another placed widget's art
						const double l = t.baseDX + ox, tp = t.baseDY + oy;
						const Rect me{ true, l, tp, l + t.dw, tp + t.dh };
						double sdx, sdy;
						SnapDeltas(a_s, i, me, rects, a_s.snapDistance, sdx, sdy);
						ox += sdx;
						oy += sdy;
					}
				}
				if (all[i].moveViaSlot && ApplySlotShift(w, t, ox, oy)) {
					ox = 0.0;   // moved through the slot; the render transform carries no translation of ours
					oy = 0.0;
				}
				const double wantX = t.baseX + ox, wantY = t.baseY + oy, wantSX = t.baseSX * scaleX, wantSY = t.baseSY * scaleY;
				const bool   atBase = ox == 0.0 && oy == 0.0 && scaleX == 1.0 && scaleY == 1.0;
				// grow and shrink about the element's own centre, as the Skyrim mod does - or, for a resource bar, about one
				// end ("Grows toward": the bar anchored on the other side, the owner 2026-09-29)
				const double pivotWantX = all[i].stat && e.grow == 1 ? 0.0 : all[i].stat && e.grow == 2 ? 1.0 : 0.5;
				if (!atBase && (!t.pivotSet || t.pivotAppliedX != pivotWantX)) {
					if (!t.pivotSet) {
						if (const auto* pv = ue::At<double>(w, Off(WidgetClass(), "RenderTransformPivot"))) {
							t.pivotX = pv[0];
							t.pivotY = pv[1];
						}
					}
					Call2(w, L"SetRenderTransformPivot", "Pivot", pivotWantX, 0.5);
					t.pivotAppliedX = pivotWantX;
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
				const bool   hide = (a_s.enabled && e.hide) || (a_hideOurs && t.created);   // the wait menu: the game's HUD is gone
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

				// the preview: while the page is open every element is shown, so the ones that only appear during an event
				// (status effects, the level-up bar, the enemy's health) can be placed; the game's state comes back after
				// the hold: the preview (a MODE, on until its switch goes off - it stays when the menu closes, so an element
				// the menu covers can be looked at) and "always visible" (in gameplay, for the elements the game fades or
				// hides on its own). Both show the element at full opacity and stop its fade; the preview also runs the
				// widget's own show calls. When both are off, everything the hold changed goes back (ReleaseHold).
				const bool preview = a_s.preview && a_s.enabled && !hide;
				const bool always = !hide && a_gameplay && all[i].fades && (a_s.alwaysVisible || e.alwaysVisible);
				if (preview || always) {
					if (!t.held) {
						t.held = true;
						t.visBeforeHold = Visibility(w);
						t.opacityBeforeHold = Opacity(w);
						if (!preview && !all[i].holdOn.empty()) { t.holdCalledAt = GetTickCount64(); RunPreviewCalls(w, all[i].holdOn, all[i].key); }   // e.g. the breath bar filled
					}
					if (always && !preview && !all[i].holdOn.empty() && !all[i].holdCheck.empty() && GetTickCount64() - t.holdCheckedAt >= 1000) {
						t.holdCheckedAt = GetTickCount64();
						bool hidden = false;
						for (const char* name : all[i].holdCheck) {
							if (auto* c = ObjProp(w, name); c && (Visibility(c) == kHidden || Visibility(c) == kCollapsed)) hidden = true;
						}
						if (hidden) {   // the game hid it (an equip change): shown again
							t.holdCalledAt = GetTickCount64();
							RunPreviewCalls(w, all[i].holdOn, all[i].key);
						}
					}
					if (always && !preview && !all[i].holdOn.empty() && GetTickCount64() - t.holdCalledAt >= 2000 && VeilOpacity(w, t) < 0.5f) {
						t.holdCalledAt = GetTickCount64();   // the game faded its retainer veil again (the enemy bar): shown again
						RunPreviewCalls(w, all[i].holdOn, all[i].key);
					}
					if (const auto v = Visibility(w); v == kHidden || v == kCollapsed) SetVisibility(w, kSelfHitTestInvisible);
					if (Opacity(w) < 0.999f) SetOpacity(w, 1.0f);
					bool      stopped = false;
					const int heldCount = all[i].holdSubtree ? HoldSubtreeVisible(w, 0, stopped, &t) : 0;
					if (always && (heldCount > 0 || stopped) && !t.forced) {
						logger::info("hud: {} always visible - {} widget(s) held at full opacity{}", all[i].key, heldCount, stopped ? ", its fade-out stopped" : "");
					}
					t.forced = always;
					if (preview) {
						t.previewWas = true;
						RaiseVeil(w, t);
						if (!all[i].previewOn.empty()) {
							// the widget's own show calls: ONCE when the preview starts, and again only if the game has hidden
							// the widget since (its own timers), at most every two seconds - a call a second replayed every
							// appear animation (the owner, 2026-09-29)
							const ULONGLONG nowMs = GetTickCount64();
							const bool hiddenAgain = t.previewCalledAt != 0 && (Visibility(w) == kHidden || Visibility(w) == kCollapsed || Opacity(w) < 0.5f || VeilOpacity(w, t) < 0.5f);
							if (t.previewCalledAt == 0 || (hiddenAgain && nowMs - t.previewCalledAt >= 2000)) {
								if (t.mutedSounds.empty()) {
									MuteSounds(w, t);
									if (!t.mutedSounds.empty()) logger::info("preview: {} - {} sound event(s) silenced while it is shown", all[i].key, t.mutedSounds.size());
								}
								t.previewCalledAt = nowMs;
								RunPreviewCalls(w, all[i].previewOn, all[i].key);
							}
						}
					} else if (t.previewWas) {
						t.previewWas = false;   // the preview ended while always-visible holds on: the show calls' state goes back
						t.previewCalledAt = 0;
						LowerVeil(t);
						if (!all[i].previewOff.empty()) RunPreviewCalls(w, all[i].previewOff, all[i].key);
						if (!all[i].holdOn.empty()) { t.holdCalledAt = GetTickCount64(); RunPreviewCalls(w, all[i].holdOn, all[i].key); }
						UnmuteSounds(t);
					}
				} else if (t.held) {
					t.held = false;
					if (t.previewWas) {
						t.previewWas = false;
						t.previewCalledAt = 0;
						if (!all[i].previewOff.empty()) RunPreviewCalls(w, all[i].previewOff, all[i].key);   // the game's own state again
						LowerVeil(t);
					}
					if (t.forced && !all[i].holdOff.empty()) RunPreviewCalls(w, all[i].holdOff, all[i].key);   // the game's own fade again
					ReleaseHold(w, t);
					UnmuteSounds(t);   // after the off calls, so those are silent too
					if (t.forced) logger::info("hud: {} back to the game's own showing and hiding", all[i].key);
					t.forced = false;
				}

				if (all[i].createNative) UpdateIndicator(w, t, all[i], a_gameplay && !hide, a_s.preview && a_s.enabled && !hide, a_s.elements[i].radius);
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
				st.placedByMinimap = minimapOwns;
				st.measured = t.measured;
				st.vx = t.vx;
				st.vy = t.vy;
				st.vw = t.vw;
				st.vh = t.vh;
				st.viewW = g_viewW;
				st.viewH = g_viewH;
				st.baseVX = t.baseVX;
				st.insetX = (1.0 - all[i].visibleW) * 0.5 * t.vw;
				st.baseVY = t.baseVY;
				st.drawn = t.drawn;
				st.dvx = t.dx;
				st.dvy = t.dy;
				st.dvw = t.dw;
				st.dvh = t.dh;
				st.baseDX = t.baseDX;
				st.baseDY = t.baseDY;
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
		const bool waiting = WaitMenuUp();
		const bool gameplay = im && im->menuMode == 1 && !waiting;   // Oblivion Remastered: 1 is gameplay (logic library, menuMode entry)
		Apply(settings::Snapshot(), gameplay, waiting);
	}

	bool OffsetRange(const ElementStatus& a_st, double a_withX, double a_withY, double& a_minX, double& a_maxX, double& a_minY, double& a_maxY)
	{
		if (!a_st.measured || a_st.viewW <= 0.0 || a_st.viewH <= 0.0) {
			return false;
		}
		// baseVX is the rectangle's left with NO offset at all; what it moves with is added, the slider's own part is not
		if (a_st.drawn && a_st.dvw > 0.0 && a_st.dvh > 0.0) {
			// the art's rectangle bounds the sliders: the strip reaches the edge, not its box
			const double leftAtZero = a_st.baseDX + a_withX, topAtZero = a_st.baseDY + a_withY;
			a_minX = -leftAtZero;
			a_maxX = std::max(a_minX, a_st.viewW - leftAtZero - a_st.dvw);
			a_minY = -topAtZero;
			a_maxY = std::max(a_minY, a_st.viewH - topAtZero - a_st.dvh);
			return true;
		}
		const double leftAtZero = a_st.baseVX + a_withX;   // the rectangle's left with the slider at 0
		const double topAtZero = a_st.baseVY + a_withY;
		a_minX = -(leftAtZero + a_st.insetX);
		a_maxX = std::max(a_minX, a_st.viewW - leftAtZero - a_st.vw + a_st.insetX);
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

	bool OffsetRange(const std::vector<ElementStatus>& a_all, std::size_t a_i, const settings::Values& a_s, double a_withX, double a_withY,
		double a_ownX, double a_ownY, double& a_minX, double& a_maxX, double& a_minY, double& a_maxY)
	{
		if (a_i >= a_all.size() || !OffsetRange(a_all[a_i], a_withX, a_withY, a_minX, a_maxX, a_minY, a_maxY)) {
			return false;
		}
		if (!a_s.noOverlap || a_s.elements.size() < a_all.size()) {
			return true;
		}
		const auto& st = a_all[a_i];
		if (!st.drawn) return true;
		std::vector<Rect> rects(a_all.size());
		for (std::size_t j = 0; j < a_all.size(); ++j) {
			const auto& o = a_all[j];
			if (o.found && o.measured && o.drawn) rects[j] = { true, o.dvx, o.dvy, o.dvx + o.dvw, o.dvy + o.dvh };
		}
		const Rect me{ true, st.dvx, st.dvy, st.dvx + st.dvw, st.dvy + st.dvh };
		double lMin, lMax, tMin, tMax;
		NeighbourLimits(a_s, a_i, me, rects, lMin, lMax, tMin, tMax);
		const double leftAtZero = st.baseDX + a_withX, topAtZero = st.baseDY + a_withY;   // the ART's left with the slider at 0
		if (lMin <= lMax) {
			a_minX = std::max(a_minX, lMin - leftAtZero);
			a_maxX = std::min(a_maxX, lMax - leftAtZero);
		}
		if (tMin <= tMax) {
			a_minY = std::max(a_minY, tMin - topAtZero);
			a_maxY = std::min(a_maxY, tMax - topAtZero);
		}
		// the slider's current value always stays inside its range (a neighbour that arrived later never throws it)
		a_minX = std::min(a_minX, a_ownX);
		a_maxX = std::max(a_maxX, a_ownX);
		a_minY = std::min(a_minY, a_ownY);
		a_maxY = std::max(a_maxY, a_ownY);
		return true;
	}

	void PageDrawn()
	{
		g_pageDrawnAt.store(GetTickCount64(), std::memory_order_relaxed);
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
												{ "forced_visible", s.forcedVisible }, { "placed_by_minimap", s.placedByMinimap }, { "measured", s.measured },
												{ "rect", { s.vx, s.vy, s.vw, s.vh } }, { "drawn", s.drawn ? json{ s.dvx, s.dvy, s.dvw, s.dvh } : json(nullptr) }, { "viewport", { s.viewW, s.viewH } } }
									  : json{ { "found", false } };
		}
		return { { "hud_found", HudFound() }, { "elements", els } };
	}
}
