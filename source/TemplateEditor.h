/* TemplateEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef TEMPLATE_EDITOR_H_
#define TEMPLATE_EDITOR_H_

#include "Audio.h"
#include "Body.h"
#include "DataWriter.h"
#include "Effect.h"
#include "Fleet.h"
#include "GameData.h"
#include "Minable.h"
#include "Sound.h"
#include "Sprite.h"
#include "SpriteSet.h"
#include "System.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_stdlib.h"

#include <list>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

class Editor;



void AddNode(Editor &editor, const std::string &file, const std::string &key, const std::string &name);



template <typename T> constexpr const char *defaultFileFor() = delete;
template <> constexpr const char *defaultFileFor<Effect>() { return "effects.txt"; }
template <> constexpr const char *defaultFileFor<Fleet>() { return "fleets.txt"; }
template <> constexpr const char *defaultFileFor<Hazard>() { return "hazards.txt"; }
template <> constexpr const char *defaultFileFor<Government>() { return "governments.txt"; }
template <> constexpr const char *defaultFileFor<Outfit>() { return "outfits.txt"; }
template <> constexpr const char *defaultFileFor<Sale<Outfit>>() { return "outfitters.txt"; }
template <> constexpr const char *defaultFileFor<Planet>() { return "map.txt"; }
template <> constexpr const char *defaultFileFor<Ship>() { return "ships.txt"; }
template <> constexpr const char *defaultFileFor<Sale<Ship>>() { return "shipyards.txt"; }
template <> constexpr const char *defaultFileFor<System>() { return "map.txt"; }

template <typename T> constexpr const char *keyFor() = delete;
template <> constexpr const char *keyFor<Effect>() { return "effect"; }
template <> constexpr const char *keyFor<Fleet>() { return "fleet"; }
template <> constexpr const char *keyFor<Hazard>() { return "hazard"; }
template <> constexpr const char *keyFor<Government>() { return "government"; }
template <> constexpr const char *keyFor<Outfit>() { return "outfit"; }
template <> constexpr const char *keyFor<Sale<Outfit>>() { return "outfitter"; }
template <> constexpr const char *keyFor<Planet>() { return "planet"; }
template <> constexpr const char *keyFor<Ship>() { return "ship"; }
template <> constexpr const char *keyFor<Sale<Ship>>() { return "shipyard"; }
template <> constexpr const char *keyFor<System>() { return "system"; }

// Base class common for any editor window.
template <typename T>
class TemplateEditor {
public:
	TemplateEditor(Editor &editor, bool &show) noexcept
		: editor(editor), show(show) {}
	TemplateEditor(const TemplateEditor &) = delete;
	TemplateEditor& operator=(const TemplateEditor &) = delete;

	const std::list<T> &Changes() const { return changes; }
	const std::set<const T *> &Dirty() const { return dirty; }

	// Saves the specified object.
	void WriteToPlugin(const T *object, bool useDefault = true) { WriteToPlugin(object, useDefault, 0); }
	template<typename U>
	void WriteToPlugin(const U *object, bool useDefault, ...)
	{
		dirty.erase(object);
		for(auto &&obj : changes)
			if(obj.Name() == object->Name())
			{
				obj = *object;
				return;
			}
		if(useDefault)
			AddNode(editor, defaultFileFor<T>(), keyFor<T>(), object->Name());
		changes.push_back(*object);
	}
	template <typename U>
	void WriteToPlugin(const U *object, bool useDefault, typename std::decay<decltype(std::declval<U>().TrueName())>::type *)
	{
		dirty.erase(object);
		for(auto &&obj : changes)
			if(obj.TrueName() == object->TrueName())
			{
				obj = *object;
				return;
			}
		if(useDefault)
			AddNode(editor, defaultFileFor<T>(), keyFor<T>(), object->TrueName());
		changes.push_back(*object);
	}
	// Saves every unsaved object.
	void WriteAll()
	{
		auto copy = dirty;
		for(auto &&obj : copy)
			WriteToPlugin(obj);
	}

protected:
	// Marks the current object as dirty.
	void SetDirty() { dirty.insert(object); }
	void SetDirty(const T *obj) { dirty.insert(obj); }
	bool IsDirty() { return dirty.count(object); }
	void SetClean() { dirty.erase(object); }

	void RenderSprites(const std::string &name, std::vector<std::pair<Body, int>> &map);
	bool RenderElement(Body *sprite, const std::string &name);
	void RenderSound(const std::string &name, std::map<const Sound *, int> &map);
	void RenderEffect(const std::string &name, std::map<const Effect *, int> &map);


protected:
	Editor &editor;
	bool &show;

	std::string searchBox;
	T *object = nullptr;


private:
	std::set<const T *> dirty;
	std::list<T> changes;
};



template <typename T>
void TemplateEditor<T>::RenderSprites(const std::string &name, std::vector<std::pair<Body, int>> &map)
{
	auto found = map.end();
	int index = 0;
	ImGui::PushID(name.c_str());
	for(auto it = map.begin(); it != map.end(); ++it)
	{
		ImGui::PushID(index++);
		if(RenderElement(&it->first, name))
			found = it;
		ImGui::PopID();
	}
	ImGui::PopID();

	if(found != map.end())
	{
		map.erase(found);
		SetDirty();
	}
}



template <typename T>
bool TemplateEditor<T>::RenderElement(Body *sprite, const std::string &name)
{
	static std::string spriteName;
	spriteName.clear();
	bool open = ImGui::TreeNode(name.c_str(), "%s: %s", name.c_str(), sprite->GetSprite() ? sprite->GetSprite()->Name().c_str() : "");
	bool value = false;
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Remove"))
			value = true;
		ImGui::EndPopup();
	}

	if(open)
	{
		if(sprite->GetSprite())
			spriteName = sprite->GetSprite()->Name();
		if(ImGui::InputCombo("sprite", &spriteName, &sprite->sprite, SpriteSet::GetSprites()))
			SetDirty();

		double value = sprite->frameRate * 60.;
		if(ImGui::InputDoubleEx("frame rate", &value, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			sprite->frameRate = value / 60.;
			SetDirty();
		}

		if(ImGui::InputInt("delay", &sprite->delay))
			SetDirty();
		if(ImGui::Checkbox("random start frame", &sprite->randomize))
			SetDirty();
		bool bvalue = !sprite->repeat;
		if(ImGui::Checkbox("no repeat", &bvalue))
			SetDirty();
		sprite->repeat = !bvalue;
		if(ImGui::Checkbox("rewind", &sprite->rewind))
			SetDirty();
		ImGui::TreePop();
	}

	return value;
}



template <typename T>
void TemplateEditor<T>::RenderSound(const std::string &name, std::map<const Sound *, int> &map)
{
	static std::string soundName;
	soundName.clear();

	bool open = ImGui::TreeNode(name.c_str());
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Sound"))
		{
			map.emplace(nullptr, 1);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(open)
	{
		const Sound *toAdd = nullptr;
		auto toRemove = map.end();
		int index = 0;
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			ImGui::PushID(index++);

			soundName = it->first ? it->first->Name() : "";
			if(ImGui::InputCombo("##sound", &soundName, &toAdd, Audio::GetSounds()))
			{
				toRemove = it;
				SetDirty();
			}
			ImGui::SameLine();
			if(ImGui::InputInt("", &it->second))
			{
				if(!it->second)
					toRemove = it;
				SetDirty();
			}
			ImGui::PopID();
		}

		if(toAdd)
		{
			map[toAdd] += map[toRemove->first];
			map.erase(toRemove);
		}
		else if(toRemove != map.end())
			map.erase(toRemove);
		ImGui::TreePop();
	}
}



template <typename T>
void TemplateEditor<T>::RenderEffect(const std::string &name, std::map<const Effect *, int> &map)
{
	static std::string effectName;
	effectName.clear();

	if(ImGui::TreeNode(name.c_str()))
	{
		Effect *toAdd = nullptr;
		auto toRemove = map.end();
		int index = 0;
		for(auto it = map.begin(); it != map.end(); ++it)
		{
			ImGui::PushID(index++);

			effectName = it->first->Name();
			if(ImGui::InputCombo("##effect", &effectName, &toAdd, GameData::Effects()))
			{
				toRemove = it;
				SetDirty();
			}
			ImGui::SameLine();
			if(ImGui::InputInt("", &it->second))
			{
				if(!it->second)
					toRemove = it;
				SetDirty();
			}
			ImGui::PopID();
		}

		static std::string newEffectName;
		static const Effect *newEffect;
		if(ImGui::InputCombo("new effect", &newEffectName, &newEffect, GameData::Effects()))
			if(!newEffectName.empty())
			{
				++map[newEffect];
				newEffectName.clear();
				newEffect = nullptr;
				SetDirty();
			}
		if(toAdd)
		{
			map[toAdd] += map[toRemove->first];
			map.erase(toRemove);
		}
		else if(toRemove != map.end())
			map.erase(toRemove);
		ImGui::TreePop();
	}
}



inline const std::string &NameFor(const std::string &obj) { return obj; }
inline const std::string &NameFor(const std::string *obj) { return *obj; }
template <typename T>
const std::string &NameFor(const T &obj) { return obj.Name(); }
template <typename T>
const std::string &NameFor(const T *obj) { return obj->Name(); }
template <typename T, typename U>
std::string NameFor(const std::pair<T, U> &obj) { return obj.first; }
inline const std::string &NameFor(const System::Asteroid &obj) { return obj.Type() ? obj.Type()->Name() : obj.Name(); }

template <typename T>
bool Count(const std::set<T> &container, const T &obj) { return container.count(obj); }
template <typename T>
bool Count(const std::vector<T> &container, const T &obj) { return std::find(container.begin(), container.end(), obj) != container.end(); }

template <typename T>
void Insert(std::set<T> &container, const T &obj) { container.insert(obj); }
template <typename T>
void Insert(std::vector<T> &container, const T &obj) { container.push_back(obj); }

template <typename T>
void AdditionalCalls(DataWriter &writer, const T &obj) {}
inline void AdditionalCalls(DataWriter &writer, const System::Asteroid &obj)
{
	writer.WriteToken(obj.Count());
	writer.WriteToken(obj.Energy());
}
inline void AdditionalCalls(DataWriter &writer, const System::FleetProbability &obj)
{
	writer.WriteToken(obj.Period());
}
inline void AdditionalCalls(DataWriter &writer, const System::HazardProbability &obj)
{
	writer.WriteToken(obj.Period());
}

template <template<typename, typename...> class C, typename T, typename ...Ts>
void WriteDiff(DataWriter &writer, const char *name, const C<T, Ts...> &orig, const C<T, Ts...> *diff, bool onOneLine = false, bool allowRemoving = true, bool implicitAdd = false, bool sorted = false)
{
	using U = typename std::remove_pointer<T>::type;
	auto writeRaw = onOneLine
		? [](DataWriter &writer, const char *name, const C<T, Ts...> &list, bool sorted)
		{
			writer.WriteToken(name);
			if(sorted)
				WriteSorted(list, [](const U *lhs, const U *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer](const U &element)
						{
							writer.WriteToken(NameFor(element));
							AdditionalCalls(writer, element);
						});
			else
				for(auto &&it : list)
				{
					writer.WriteToken(NameFor(it));
					AdditionalCalls(writer, it);
				}
			writer.Write();
		}
		: [](DataWriter &writer, const char *name, const C<T, Ts...> &list, bool sorted)
		{
			if(sorted)
				WriteSorted(list, [](const U *lhs, const U *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer, &name](const U &element)
						{
							writer.WriteToken(name);
							writer.WriteToken(NameFor(element));
							AdditionalCalls(writer, element);
							writer.Write();
						});
			else
				for(auto &&it : list)
				{
					writer.WriteToken(name);
					writer.WriteToken(NameFor(it));
					AdditionalCalls(writer, it);
					writer.Write();
				}
		};
	auto writeAdd = onOneLine
		? [](DataWriter &writer, const char *name, const C<T, Ts...> &list, bool sorted)
		{
			writer.WriteToken("add");
			writer.WriteToken(name);
			if(sorted)
				WriteSorted(list, [](const U *lhs, const U *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer](const U &element)
						{
							writer.WriteToken(NameFor(element));
							AdditionalCalls(writer, element);
						});
			else
				for(auto &&it : list)
				{
					writer.WriteToken(NameFor(it));
					AdditionalCalls(writer, it);
				}
			writer.Write();
		}
		: [](DataWriter &writer, const char *name, const C<T, Ts...> &list, bool sorted)
		{
			if(sorted)
				WriteSorted(list, [](const U *lhs, const U *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer, &name](const U &element)
						{
							writer.WriteToken("add");
							writer.WriteToken(name);
							writer.WriteToken(NameFor(element));
							AdditionalCalls(writer, element);
							writer.Write();
						});
			else
				for(auto &&it : list)
				{
					writer.WriteToken("add");
					writer.WriteToken(name);
					writer.WriteToken(NameFor(it));
					AdditionalCalls(writer, it);
					writer.Write();
				}
		};
	auto writeRemove = onOneLine
		? [](DataWriter &writer, const char *name, const C<T, Ts...> &list, bool sorted)
		{
			writer.WriteToken("remove");
			writer.WriteToken(name);
			if(sorted)
				WriteSorted(list, [](const U *lhs, const U *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer](const U &element)
						{
							writer.Write(NameFor(element));
						});
			else
				for(auto &&it : list)
					writer.WriteToken(NameFor(it));
			writer.Write();
		}
		: [](DataWriter &writer, const char *name, const C<T, Ts...> &list, bool sorted)
		{
			if(sorted)
				WriteSorted(list, [](const U *lhs, const U *rhs) { return NameFor(lhs) < NameFor(rhs); },
						[&writer, &name](const U &element)
						{
							writer.Write("remove", name, NameFor(element));
						});
			else
				for(auto &&it : list)
					writer.Write("remove", name, NameFor(it));
		};
	if(!diff)
	{
		if(!orig.empty())
			writeRaw(writer, name, orig, sorted);
		return;
	}

	typename std::decay<decltype(orig)>::type toAdd;
	auto toRemove = toAdd;

	for(auto &&it : orig)
		if(!Count(*diff, it))
			Insert(toAdd, it);
	for(auto &&it : *diff)
		if(!Count(orig, it))
			Insert(toRemove, it);

	if(toAdd.empty() && toRemove.empty())
		return;

	if(toRemove.size() == diff->size() && !diff->empty())
	{
		if(orig.empty())
			writer.Write("remove", name);
		else
			writeRaw(writer, name, toAdd, sorted);
	}
	else if(allowRemoving)
	{
		if(!toAdd.empty())
		{
			if(implicitAdd)
				writeRaw(writer, name, toAdd, sorted);
			else
				writeAdd(writer, name, toAdd, sorted);
		}
		if(!toRemove.empty())
			writeRemove(writer, name, toRemove, sorted);
	}
	else if(!orig.empty())
		writeRaw(writer, name, orig, sorted);
}



#endif
