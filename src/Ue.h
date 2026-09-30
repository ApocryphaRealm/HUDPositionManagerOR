#pragma once

// ============================================================================================================
// Just enough Unreal to read the camera (game thread only). CommonLibOB64 has no FProperty, so a property is found
// by NAME by walking a struct's FField chain (next +0x18, name +0x20) and reading FProperty::Offset_Internal at +0x44
// (UE5's layout) - SelfCheck() proves that offset on a property whose place is already known before anything else
// trusts it. Taken from Tween Menu for Oblivion's Reflect/Ue helpers (logic library 7701, 7720), trimmed to what this
// mod reads. Every look-up here is cached by the caller: FName::ToString allocates.
// ============================================================================================================

namespace ue
{
	inline std::wstring Widen(const std::string& a_utf8)
	{
		const int n = ::MultiByteToWideChar(CP_UTF8, 0, a_utf8.data(), static_cast<int>(a_utf8.size()), nullptr, 0);
		std::wstring out(n > 0 ? static_cast<std::size_t>(n) : 0, L'\0');
		if (n > 0) ::MultiByteToWideChar(CP_UTF8, 0, a_utf8.data(), static_cast<int>(a_utf8.size()), out.data(), n);
		return out;
	}
	inline std::string Utf8FromWide(const wchar_t* a_wide)
	{
		if (!a_wide) return {};
		const int n = ::WideCharToMultiByte(CP_UTF8, 0, a_wide, -1, nullptr, 0, nullptr, nullptr);
		std::string out(n > 1 ? static_cast<std::size_t>(n - 1) : 0, '\0');
		if (n > 1) ::WideCharToMultiByte(CP_UTF8, 0, a_wide, -1, out.data(), n - 1, nullptr, nullptr);
		return out;
	}

	std::string Utf8(const UE::FString& a_s);
	std::string NameOf(UE::UObject* a_o);

	// -1 when the struct (or its supers) has no property of that name
	std::int32_t Offset(UE::UStruct* a_struct, std::string_view a_name);

	// the offsets of every object property (the struct and its supers) whose class is a_className - e.g. "AkAudioEvent"
	// for a widget's sound events (2026-09-29, the preview's silence)
	std::vector<std::int32_t> ObjectPropertiesOfClass(UE::UStruct* a_struct, std::string_view a_className);

	bool SelfCheck();   // true once the Offset_Internal layout is proven

	// For an object read THIS frame from a live owner: reads a_o's own index, so never for a pointer kept from an
	// earlier frame (a freed object's index is garbage - Improved Wheel Menu crashed in exactly that read, 09:15).
	bool IsLive(UE::UObject* a_o);
	// an object being destroyed, garbage or unreachable: never a world context (2026-09-30, Minimap Menu's crash on quit)
	bool Dying(UE::UObject* a_o);
	// ProcessEvent inside __try/__except: a call that faults (a world torn down under its context) returns false
	bool GuardedProcessEvent(UE::UObject* a_obj, UE::UFunction* a_fn, void* a_params);

	// A pointer kept across frames with the object-array slot it was found in. Get() asks the SLOT whether it still
	// holds that object, and then that the object there still has the class and name seen at Set(): a freed widget's
	// slot AND address were both reused by another object, and the dead one passed as alive (the Level text: a
	// full-screen rectangle and sliders stuck at 0 %, 2026-09-29).
	struct Handle
	{
		UE::UObject* ptr = nullptr;
		std::int32_t index = -1;
		UE::UClass*  cls = nullptr;
		std::uint64_t name = 0;   // the FName's 8 bytes
		void         Set(UE::UObject* a_live);   // a_live must be live now (just found)
		UE::UObject* Get() const;                // nullptr once the slot holds anything else
	};

	// the first live object whose class is a_base or derives from it (not a class default object) - scans the whole
	// object array, so the caller caches the answer
	UE::UObject* FirstOf(UE::UClass* a_base);

	inline UE::UClass* Class(const wchar_t* a_path)
	{
		return UE::StaticFindObject<UE::UClass>(nullptr, nullptr, a_path);
	}

	template <class T>
	T* At(void* a_base, std::int32_t a_offset)
	{
		return a_base && a_offset >= 0 ? reinterpret_cast<T*>(static_cast<std::uint8_t*>(a_base) + a_offset) : nullptr;
	}

	// A reflected call: parameters by name, laid out from the UFunction's own properties (Tween Menu's ue::Call). The
	// UFunction is looked up per call - callers that run every frame keep their own static Call per (class, function).
	class Call
	{
	public:
		Call(UE::UObject* a_obj, const wchar_t* a_fn) :
			m_obj(a_obj),
			m_fn(a_obj ? a_obj->FindFunction(UE::FName(a_fn, UE::EFindName::Find)) : nullptr)
		{
			if (m_fn) {
				m_params.assign(static_cast<std::size_t>(reinterpret_cast<UE::UStruct*>(m_fn)->propertiesSize) + 16, 0);
			}
		}
		explicit operator bool() const { return m_fn != nullptr; }
		UE::UFunction* Function() const { return m_fn; }
		void* At(std::string_view a_name)
		{
			if (!m_fn) {
				return nullptr;
			}
			const auto off = Offset(reinterpret_cast<UE::UStruct*>(m_fn), a_name);
			return off >= 0 ? m_params.data() + off : nullptr;
		}
		template <class T>
		bool Set(std::string_view a_name, const T& a_value)
		{
			if (void* p = At(a_name)) {
				std::memcpy(p, &a_value, sizeof(T));
				return true;
			}
			return false;
		}
		bool RunGuarded()
		{
			return m_fn && m_obj && GuardedProcessEvent(m_obj, m_fn, m_params.data());
		}

		bool Run()
		{
			if (!m_fn || !m_obj) {
				return false;
			}
			m_obj->ProcessEvent(m_fn, m_params.data());
			return true;
		}

	private:
		UE::UObject*              m_obj;
		UE::UFunction*            m_fn;
		std::vector<std::uint8_t> m_params;
	};

	// A reflected call with no parameters in and one ReturnValue out, laid out from the UFunction's own properties.
	// The UFunction and its ReturnValue offset are looked up once per (class, name) by the caller's static.
	class Getter
	{
	public:
		Getter(const wchar_t* a_function) :
			m_name(a_function)
		{}

		// false when the object has no such function; a_out gets sizeof(T) bytes from ReturnValue
		template <class T>
		bool Get(UE::UObject* a_obj, T& a_out)
		{
			if (!a_obj || !Resolve(a_obj)) {
				return false;
			}
			m_params.assign(m_params.size(), 0);
			a_obj->ProcessEvent(m_fn, m_params.data());
			std::memcpy(&a_out, m_params.data() + m_ret, sizeof(T));
			return true;
		}

	private:
		bool Resolve(UE::UObject* a_obj);

		const wchar_t*            m_name;
		UE::UClass*               m_class = nullptr;
		UE::UFunction*            m_fn = nullptr;
		std::int32_t              m_ret = -1;
		std::vector<std::uint8_t> m_params;
	};
}
