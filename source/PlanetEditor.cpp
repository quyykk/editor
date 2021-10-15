/* PlanetEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "PlanetEditor.h"

#include "DataFile.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Dialog.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "Editor.h"
#include "Effect.h"
#include "Engine.h"
#include "Files.h"
#include "GameData.h"
#include "Government.h"
#include "Hazard.h"
#include "MainPanel.h"
#include "MainEditorPanel.h"
#include "MapPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Ship.h"
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"

#include <cassert>
#include <map>

using namespace std;



PlanetEditor::PlanetEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Planet>(editor, show)
{
}



void PlanetEditor::Select(const Planet *planet)
{
	object = const_cast<Planet *>(planet);
}



void PlanetEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Planet Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(object && editor.HasPlugin() && IsDirty()
			&& ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_S))
		WriteToPlugin(object);

	bool showNewPlanet = false;
	bool showRenamePlanet = false;
	bool showClonePlanet = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Planet"))
		{
			const bool alreadyDefined = object && !GameData::basePlanets.Has(object->TrueName());
			ImGui::MenuItem("New", nullptr, &showNewPlanet);
			ImGui::MenuItem("Rename", nullptr, &showRenamePlanet, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showClonePlanet, object);
			if(ImGui::MenuItem("Save", nullptr, false, object && editor.HasPlugin() && IsDirty()))
				WriteToPlugin(object);
			if(ImGui::MenuItem("Reset", nullptr, false, object && IsDirty()))
			{
				SetClean();
				bool found = false;
				for(auto &&change : Changes())
					if(change.TrueName() == object->TrueName())
					{
						*object = change;
						found = true;
						break;
					}

				if(!found && GameData::basePlanets.Has(object->name))
					*object = *GameData::basePlanets.Get(object->TrueName());
				else if(!found)
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
					GameData::Planets().Erase(object->name);
					object = nullptr;
				}
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				if(find_if(Changes().begin(), Changes().end(), [this](const Planet &planet)
							{
								return planet.name == object->name;
							}) != Changes().end())
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
				}
				else
					SetClean();
				GameData::Planets().Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewPlanet)
		ImGui::OpenPopup("New Planet");
	if(showRenamePlanet)
		ImGui::OpenPopup("Rename Planet");
	if(showClonePlanet)
		ImGui::OpenPopup("Clone Planet");
	ImGui::BeginSimpleNewModal("New Planet", [this](const string &name)
			{
				auto *newPlanet = const_cast<Planet *>(GameData::Planets().Get(name));
				newPlanet->name = name;
				newPlanet->isDefined = true;
				object = newPlanet;

				// If the in system editor is open and a stellar object without a planet is selected,
				// then we assign the new planet to that stellar.
				if(auto *panel = dynamic_cast<MainEditorPanel *>(editor.GetMenu().Top().get()))
					if(auto *stellar = panel->SelectedObject())
						if(!stellar->planet)
						{
							const_cast<StellarObject *>(stellar)->planet = object;
							newPlanet->SetSystem(panel->Selected());
						}
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Planet", [this](const string &name)
			{
				DeleteFromChanges();
				editor.RenameObject(keyFor<Planet>(), object->name, name);
				GameData::Planets().Rename(object->name, name);
				object->name = name;
				WriteToPlugin(object, false);
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Planet", [this](const string &name)
			{
				auto *clone = const_cast<Planet *>(GameData::Planets().Get(name));
				*clone = *object;
				object = clone;

				object->name = name;
				object->systems.clear();
				SetDirty();
			});

	if(ImGui::InputCombo("planet", &searchBox, &object, GameData::Planets()))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	if(object)
		RenderPlanet();
	ImGui::End();
}



void PlanetEditor::RenderPlanet()
{
	int index = 0;

	ImGui::Text("name: %s", object->name.c_str());

	if(ImGui::TreeNode("attributes"))
	{
		set<string> toAdd;
		set<string> toRemove;
		for(auto &attribute : object->attributes)
		{
			if(attribute == "spaceport" || attribute == "shipyard" || attribute == "outfitter")
				continue;

			ImGui::PushID(index++);
			string str = attribute;
			if(ImGui::InputText("##attribute", &str, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(!str.empty())
					toAdd.insert(move(str));
				toRemove.insert(attribute);
			}
			ImGui::PopID();
		}
		for(auto &&attribute : toAdd)
		{
			object->attributes.insert(attribute);
			if(attribute.size() > 10 && attribute.compare(0, 10, "requires: "))
				object->requiredAttributes.insert(attribute.substr(10));
		}
		for(auto &&attribute : toRemove)
		{
			object->attributes.erase(attribute);
			if(attribute.size() > 10 && attribute.compare(0, 10, "requires: "))
				object->requiredAttributes.erase(attribute.substr(10));
		}
		if(!toAdd.empty() || !toRemove.empty())
			SetDirty();

		ImGui::Spacing();

		static string addAttribute;
		if(ImGui::InputText("##planet", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
			if(!addAttribute.empty())
			{
				object->attributes.insert(addAttribute);
				addAttribute.clear();
				SetDirty();
			}
		if(!addAttribute.empty() && !ImGui::IsInputFocused("##planet"))
		{
			object->attributes.insert(addAttribute);
			addAttribute.clear();
			SetDirty();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("shipyards"))
	{
		index = 0;
		const Sale<Ship> *toAdd = nullptr;
		const Sale<Ship> *toRemove = nullptr;
		for(auto it = object->shipSales.begin(); it != object->shipSales.end(); ++it)
		{
			ImGui::PushID(index++);
			static Sale<Ship> *selected;
			string name = (*it)->name.c_str();
			if(ImGui::InputCombo("shipyard", &name, &selected, GameData::Shipyards()))
			{
				if(selected)
					toAdd = selected;
				toRemove = *it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(toAdd)
			object->shipSales.insert(toAdd);
		if(toRemove)
			object->shipSales.erase(toRemove);
		static Sale<Ship> *selected;
		string name;
		if(ImGui::InputCombo("add shipyard", &name, &selected, GameData::Shipyards()))
			if(selected)
			{
				object->shipSales.insert(selected);
				SetDirty();
			}
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("outfitters"))
	{
		index = 0;
		const Sale<Outfit> *toAdd = nullptr;
		const Sale<Outfit> *toRemove = nullptr;
		for(auto it = object->outfitSales.begin(); it != object->outfitSales.end(); ++it)
		{
			ImGui::PushID(index++);
			static Sale<Outfit> *selected;
			string name = (*it)->name.c_str();
			if(ImGui::InputCombo("outfitter", &name, &selected, GameData::Outfitters()))
			{
				if(selected)
					toAdd = selected;
				toRemove = *it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(toAdd)
			object->outfitSales.insert(toAdd);
		if(toRemove)
			object->outfitSales.erase(toRemove);
		static Sale<Outfit> *selected;
		string name;
		if(ImGui::InputCombo("add outfitter", &name, &selected, GameData::Outfitters()))
			if(selected)
			{
				object->outfitSales.insert(selected);
				SetDirty();
			}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("tribute"))
	{
		if(ImGui::InputInt("tribute", &object->tribute))
			SetDirty();
		if(ImGui::InputInt("threshold", &object->defenseThreshold))
		{
			if(object->defenseThreshold < 0)
				object->defenseThreshold = 0;
			SetDirty();
		}

		int rank = static_cast<int>(log(max<int64_t>(1, object->defenseThreshold)));
		if(ImGui::InputInt("threshold rank", &rank))
		{
			if(rank < 0)
				rank = 0;
			object->defenseThreshold = static_cast<int>(exp(rank)) + 1;
			SetDirty();
		}

		if(ImGui::TreeNode("fleets"))
		{
			index = 0;
			map<const Fleet *, int> modify;
			for(auto it = object->defenseFleets.begin(); it != object->defenseFleets.end();)
			{
				int count = 1;
				auto counter = it;
				while(next(counter) != object->defenseFleets.end()
						&& (*next(counter))->Name() == (*counter)->Name())
				{
					++counter;
					++count;
				}

				ImGui::PushID(index);
				static Fleet *selected;
				string fleetName = (*it)->Name();
				if(ImGui::InputCombo("fleet", &fleetName, &selected, GameData::Fleets()))
					if(selected)
					{
						// We need to update every entry.
						auto begin = it;
						auto end = it + count;
						while(begin != end)
							*begin++ = selected;
						SetDirty();
					}

				ImGui::SameLine();
				int oldCount = count;
				if(ImGui::InputInt("fleet count", &count))
				{
					modify[*it] = count - oldCount;
					SetDirty();
				}

				++index;
				it += oldCount;
				ImGui::PopID();
			}

			// Now update any fleets entries.
			for(auto &&pair : modify)
			{
				auto first = find(object->defenseFleets.begin(), object->defenseFleets.end(), pair.first);
				if(pair.second <= 0)
					object->defenseFleets.erase(first, first + (-pair.second));
				else
					object->defenseFleets.insert(first, pair.second, pair.first);
			}

			// Now add the ability to add fleets.
			static Fleet *newFleet = nullptr;
			string addFleet;
			if(ImGui::InputCombo("add fleet", &addFleet, &newFleet, GameData::Fleets()))
				if(newFleet)
				{
					object->defenseFleets.emplace_back(newFleet);
					SetDirty();
				}

			ImGui::TreePop();
		}
		ImGui::TreePop();
	}


	string landscapeName;
	if(object->landscape)
		landscapeName = object->landscape->Name();
	if(ImGui::InputCombo("landscape", &landscapeName, &object->landscape, SpriteSet::GetSprites(),
				[](const string &name) { return !name.compare(0, 5, "land/"); }))
	{
		GameData::Preload(object->landscape);
		SetDirty();
	}
	if(ImGui::InputText("music", &object->music, ImGuiInputTextFlags_EnterReturnsTrue))
		SetDirty();

	if(ImGui::InputTextMultiline("description", &object->description))
		SetDirty();
	if(ImGui::InputTextMultiline("spaceport", &object->spaceport))
	{
		SetDirty();

		if(object->spaceport.empty())
			object->attributes.erase("spaceport");
		else
			object->attributes.insert("spaceport");
	}

	{
		static Government *selected;
		string govName = object->government ? object->government->TrueName() : "";
		if(ImGui::InputCombo("government", &govName, &selected, GameData::Governments()))
		{
			object->government = selected;
			SetDirty();
		}
	}

	if(ImGui::InputDoubleEx("required reputation", &object->requiredReputation))
		SetDirty();
	if(ImGui::InputDoubleEx("bribe", &object->bribe))
		SetDirty();
	if(ImGui::InputDoubleEx("security", &object->security))
		SetDirty();
	object->customSecurity = object->security != .25;

	object->inhabited = (!object->spaceport.empty() || object->requiredReputation || !object->defenseFleets.empty()) && !object->attributes.count("uninhabited");
}



void PlanetEditor::WriteToFile(DataWriter &writer, const Planet *planet)
{
	const auto *diff = GameData::basePlanets.Has(planet->TrueName())
		? GameData::basePlanets.Get(planet->TrueName())
		: nullptr;

	writer.Write("planet", planet->TrueName());
	writer.BeginChild();

	auto planetAttributes = planet->attributes;
	auto diffAttributes = diff ? diff->attributes : planet->attributes;
	planetAttributes.erase("spaceport");
	planetAttributes.erase("shipyard");
	planetAttributes.erase("outfitter");
	diffAttributes.erase("spaceport");
	diffAttributes.erase("shipyard");
	diffAttributes.erase("outfitter");
	WriteDiff(writer, "attributes", planetAttributes, diff ? &diffAttributes : nullptr, true);
	WriteDiff(writer, "shipyard", planet->shipSales, diff ? &diff->shipSales : nullptr, false, true, true, true);
	WriteDiff(writer, "outfitter", planet->outfitSales, diff ? &diff->outfitSales : nullptr, false, true, true, true);

	if((!diff || planet->landscape != diff->landscape) && planet->landscape)
		writer.Write("landscape", planet->landscape->Name());
	if(!diff || planet->music != diff->music)
	{
		if(!planet->music.empty())
			writer.Write("music", planet->music);
		else if(diff)
			writer.Write("remove", "music");
	}
	if(!diff || planet->description != diff->description)
	{
		if(!planet->description.empty())
		{
			auto marker = planet->description.find('\n');
			size_t start = 0;
			do
			{
				string toWrite = planet->description.substr(start, marker - start);
				writer.Write("description", toWrite);

				start = marker + 1;
				if(planet->description[start] == '\t')
					++start;
				marker = planet->description.find('\n', start);
			} while(marker != string::npos);
		}
		else if(diff)
			writer.Write("remove", "description");
	}
	if(!diff || planet->spaceport != diff->spaceport)
	{
		if(!planet->spaceport.empty())
		{
			auto marker = planet->spaceport.find('\n');
			size_t start = 0;
			do
			{
				string toWrite = planet->spaceport.substr(start, marker - start);
				writer.Write("spaceport", toWrite);

				start = marker + 1;
				if(planet->spaceport[start] == '\t')
					++start;
				marker = planet->spaceport.find('\n', start);
			} while(marker != string::npos);
		}
		else if(diff)
			writer.Write("remove", "spaceport");
	}
	if(!diff || planet->government != diff->government)
	{
		if(planet->government)
			writer.Write("government", planet->government->TrueName());
		else if(diff)
			writer.Write("remove", "government");
	}
	if(!diff || planet->requiredReputation != diff->requiredReputation)
	{
		if(planet->requiredReputation)
			writer.Write("required reputation", planet->requiredReputation);
		else if(diff)
			writer.Write("remove", "required reputation");
	}
	if(!diff || planet->bribe != diff->bribe)
	{
		if((diff && planet->bribe) || (!diff && planet->bribe != .01))
			writer.Write("bribe", planet->bribe);
		else if(diff)
			writer.Write("remove", "bribe");
	}
	if(!diff || planet->security != diff->security)
	{
		if((diff && planet->security) || (!diff && planet->security != .25))
			writer.Write("security", planet->security);
		else if(diff)
			writer.Write("remove", "security");
	}
	if(!diff || planet->tribute != diff->tribute)
	{
		if(planet->tribute)
			writer.Write("tribute", planet->tribute);
		else if(diff)
			writer.Write("remove", "tribute");
	}
	writer.BeginChild();
	if(!diff || planet->defenseThreshold != diff->defenseThreshold)
		if(planet->defenseThreshold != 4000. || diff)
			writer.Write("threshold", planet->defenseThreshold);
	if(!diff || planet->defenseFleets != diff->defenseFleets)
		for(size_t i = 0; i < planet->defenseFleets.size(); ++i)
		{
			size_t count = 1;
			while(i + 1 < planet->defenseFleets.size()
					&& planet->defenseFleets[i + 1]->Name() == planet->defenseFleets[i]->Name())
			{
				++i;
				++count;
			}
			if(count == 1)
				writer.Write("fleet", planet->defenseFleets[i]->Name());
			else
				writer.Write("fleet", planet->defenseFleets[i]->Name(), count);
		}
	writer.EndChild();

	writer.EndChild();
}
