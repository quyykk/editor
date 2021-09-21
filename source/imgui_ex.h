/* imgui_ex.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef IMGUI_EX_H_
#define IMGUI_EX_H_

#define IMGUI_DEFINE_MATH_OPERATORS

#include "Set.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>



namespace ImGui
{
	IMGUI_API bool InputDoubleEx(const char *label, double *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputFloatEx(const char *label, float *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputDouble2Ex(const char *label, double *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputInt64Ex(const char *label, int64_t *v, ImGuiInputTextFlags flags = 0);
	IMGUI_API bool InputSizeTEx(const char *label, size_t *v, ImGuiInputTextFlags flags = 0);

	template <typename T>
	IMGUI_API bool InputCombo(const char *label, std::string *input, T **element, const Set<T> &elements);
	template <typename T>
	IMGUI_API bool InputCombo(const char *label, std::string *input, const T **element, const Set<T> &elements);

	IMGUI_API bool InputSwizzle(const char *label, int *swizzle, bool allowNoSwizzle = false);

	template <typename F>
	IMGUI_API void BeginSimpleModal(const char *id, const char *label, const char *button, F &&f);
	template <typename F>
	IMGUI_API void BeginSimpleNewModal(const char *id, F &&f);
	template <typename F>
	IMGUI_API void BeginSimpleRenameModal(const char *id, F &&f);
	template <typename F>
	IMGUI_API void BeginSimpleCloneModal(const char *id, F &&f);
}




template <typename T>
bool IsValid(const T &obj, ...)
{
	return !obj.Name().empty();
}
template <typename T>
bool IsValid(const T &obj, decltype(obj.TrueName(), void()) *)
{
	return !obj.TrueName().empty();
}



template <typename T>
IMGUI_API bool ImGui::InputCombo(const char *label, std::string *input, T **element, const Set<T> &elements)
{
	ImGuiWindow *window = GetCurrentWindow();
	const auto callback = [](ImGuiInputTextCallbackData *data)
	{
		bool &autocomplete = *reinterpret_cast<bool *>(data->UserData);
		switch(data->EventFlag)
		{
		case ImGuiInputTextFlags_CallbackCompletion:
			autocomplete = true;
			break;
		}

		return 0;
	};

	const auto id = ImHashStr("##combo/popup", 0, window->GetID(label));
	bool autocomplete = false;
	bool enter = false;
	if(InputText(label, input, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackCompletion, callback, &autocomplete))
	{
		SetActiveID(0, FindWindowByID(id));
		enter = true;
	}

	bool isOpen = IsPopupOpen(id, ImGuiPopupFlags_None);
	if(IsItemActive() && !isOpen)
	{
		OpenPopupEx(id, ImGuiPopupFlags_None);
		isOpen = true;
	}

	if(!isOpen)
		return false;
	if(autocomplete && input->empty())
		autocomplete = false;

    const ImRect bb(window->DC.CursorPos,
			window->DC.CursorPos + ImVec2(CalcItemWidth(), 0.f));

	bool changed = false;
	if(BeginComboPopup(id, bb, ImGuiComboFlags_None, ImGuiWindowFlags_NoFocusOnAppearing))
	{
		BringWindowToDisplayFront(GetCurrentWindow());
		if(enter)
		{
			*element = elements.Find(*input) ? const_cast<T *>(elements.Get(*input)) : nullptr;
			CloseCurrentPopup();
			EndCombo();
			return input->empty();
		}

		std::vector<const std::string *> strings;
		strings.reserve(elements.size());
		for(auto it = elements.begin(); it != elements.end(); ++it)
			if(IsValid(it->second, 0))
				strings.push_back(&it->first);

		std::vector<std::pair<double, const char *>> weights;
		for(auto &&second : strings)
		{
			const auto generatePairs = [](const std::string &str)
			{
				std::vector<std::pair<char, char>> pair;
				for(int i = 0; i < static_cast<int>(str.size()); ++i)
					pair.emplace_back(str[i], i + 1 < static_cast<int>(str.size()) ? str[i + 1] : '\0');
				return pair;
			};
			std::vector<std::pair<char, char>> lhsPairs = generatePairs(*input);
			std::vector<std::pair<char, char>> rhsPairs = generatePairs(*second);

			int sameCount = 0;
			const auto transform = [](std::pair<char, char> c)
			{
				if(std::isalpha(c.first))
					c.first = std::tolower(c.first);
				if(std::isalpha(c.second))
					c.second = std::tolower(c.second);
				return c;
			};
			for(int i = 0, size = std::min(lhsPairs.size(), rhsPairs.size()); i < size; ++i)
			{
				const auto &lhs = transform(lhsPairs[i]);
				const auto &rhs = transform(rhsPairs[i]);
				if(lhs == rhs)
					++sameCount;
				else if(i == size - 1 && lhs.second == '\0' && lhs.first == rhs.first)
					++sameCount;
			}

			weights.emplace_back((2. * sameCount) / (lhsPairs.size() + rhsPairs.size()), second->c_str());
		}

		std::sort(weights.begin(), weights.end(),
				[](const std::pair<double, const char *> &lhs, const std::pair<double, const char *> &rhs)
					{ return lhs.first > rhs.first; });

		if(!weights.empty())
		{
			auto topWeight = weights[0].first;
			for(const auto &item : weights)
			{
				// Allow the user to select an entry in the combo box.
				// This is a hack to workaround the fact that we change the focus when clicking an
				// entry and that this means that the filtered list will change (breaking entries).
				if(GetActiveID() == GetCurrentWindow()->GetID(item.second) || GetFocusID() == GetCurrentWindow()->GetID(item.second))
				{
					*element = const_cast<T *>(elements.Get(item.second));
					changed = true;
					*input = item.second;
					CloseCurrentPopup();
					SetActiveID(0, GetCurrentWindow());
				}

				if(topWeight && item.first < topWeight * .45)
					continue;

				if(Selectable(item.second) || autocomplete)
				{
					*element = const_cast<T *>(elements.Get(item.second));
					changed = true;
					*input = item.second;
					if(autocomplete)
					{
						autocomplete = false;
						CloseCurrentPopup();
						SetActiveID(0, GetCurrentWindow());
					}
				}
			}
		}
		EndCombo();
	}

	return changed;
}


template <typename T>
IMGUI_API bool ImGui::InputCombo(const char *label, std::string *input, const T **element, const Set<T> &elements)
{
	return InputCombo(label, input, const_cast<T **>(element), elements);
}



template <typename F>
IMGUI_API void ImGui::BeginSimpleModal(const char *id, const char *label, const char *button, F &&f)
{
	if(BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		if(IsWindowAppearing())
			SetKeyboardFocusHere();
		static std::string name;
		bool create = InputText(label, &name, ImGuiInputTextFlags_EnterReturnsTrue);
		if(Button("Cancel"))
		{
			CloseCurrentPopup();
			name.clear();
		}
		SameLine();
		if(name.empty())
			PushDisabled();
		if(Button(button) || create)
		{
			std::forward<F &&>(f)(name);
			CloseCurrentPopup();
			name.clear();
		}
		else if(name.empty())
			PopDisabled();
		EndPopup();
	}
}



template <typename F>
IMGUI_API void ImGui::BeginSimpleNewModal(const char *id, F &&f)
{
	return BeginSimpleModal(id, "new name", "Create", std::forward<F &&>(f));
}



template <typename F>
IMGUI_API void ImGui::BeginSimpleRenameModal(const char *id, F &&f)
{
	return BeginSimpleModal(id, "new name", "Rename", std::forward<F &&>(f));
}



template <typename F>
IMGUI_API void ImGui::BeginSimpleCloneModal(const char *id, F &&f)
{
	return BeginSimpleModal(id, "clone name", "Clone", std::forward<F &&>(f));
}



#endif
