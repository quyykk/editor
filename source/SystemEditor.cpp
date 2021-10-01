/* SystemEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "SystemEditor.h"

#include "DataFile.h"
#include "DataWriter.h"
#include "Editor.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "GameData.h"
#include "Government.h"
#include "Hazard.h"
#include "MainEditorPanel.h"
#include "MapPanel.h"
#include "MapEditorPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"
#include "Visual.h"

#include <random>

using namespace std;



SystemEditor::SystemEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<System>(editor, show)
{
}



void SystemEditor::UpdateSystemPosition(const System *system, Point dp)
{
	const_cast<System *>(system)->position += dp;
	SetDirty(system);
}



void SystemEditor::UpdateStellarPosition(const StellarObject &object, Point dp, const System *system)
{
	auto &obj = const_cast<StellarObject &>(object);
	auto newPos = obj.position + dp;
	if(obj.parent != -1)
		newPos -= this->object->objects[obj.parent].position;
	obj.distance = newPos.Length();
	SetDirty(this->object);
}



void SystemEditor::ToggleLink(const System *system)
{
	if(system == object)
		return;

	auto it = object->links.find(system);
	if(it != object->links.end())
		object->Unlink(const_cast<System *>(system));
	else
		object->Link(const_cast<System *>(system));
	GameData::UpdateSystem(object);
	GameData::UpdateSystem(const_cast<System *>(system));
	UpdateMap();
	SetDirty();
	SetDirty(system);
}



void SystemEditor::CreateNewSystem(Point position)
{
	this->position = position;
	createNewSystem = true;
}



void SystemEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("System Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::IsWindowFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(SDL_SCANCODE_S))
		SaveCurrent();

	bool showNewSystem = false;
	bool showRenameSystem = false;
	bool showCloneSystem = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("System"))
		{
			const bool alreadyDefined = object && !GameData::baseSystems.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewSystem);
			ImGui::MenuItem("Rename", nullptr, &showRenameSystem, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneSystem, object);
			if(ImGui::MenuItem("Save", nullptr, false, object && editor.HasPlugin() && IsDirty()))
				WriteToPlugin(object);
			if(ImGui::MenuItem("Reset", nullptr, false, object && IsDirty()))
			{
				SetClean();
				auto oldLinks = object->links;
				for(auto &&link : oldLinks)
				{
					const_cast<System *>(link)->Unlink(object);
					SetDirty(link);
					GameData::UpdateSystem(const_cast<System *>(link));
				}
				for(auto &&stellar : object->Objects())
					if(stellar.planet)
						const_cast<Planet *>(stellar.planet)->RemoveSystem(object);

				auto oldNeighbors = object->VisibleNeighbors();
				bool found = false;
				for(auto &&change : Changes())
					if(change.Name() == object->Name())
					{
						*object = change;
						found = true;
						break;
					}

				if(!found && GameData::baseSystems.Has(object->name))
					*object = *GameData::baseSystems.Get(object->name);
				else if(!found)
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
					GameData::Systems().Erase(object->name);
					object = nullptr;
					if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
						panel->Select(object);
					if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
						panel->Select(object);
				}

				for(auto &&link : oldNeighbors)
					GameData::UpdateSystem(const_cast<System *>(link));

				if(object)
				{
					for(auto &&link : object->links)
					{
						const_cast<System *>(link)->Link(object);
						GameData::UpdateSystem(const_cast<System *>(link));
					}
					for(auto &&stellar : object->Objects())
						if(stellar.planet)
							const_cast<Planet *>(stellar.planet)->SetSystem(object);
					GameData::UpdateSystem(object);
					for(auto &&link : object->VisibleNeighbors())
						GameData::UpdateSystem(const_cast<System *>(link));
				}
				UpdateMap();
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
				Delete(object, true);
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Randomize"))
		{
			if(ImGui::MenuItem("Randomize Stellars", nullptr, false, object))
				Randomize();
			if(ImGui::MenuItem("Randomize Asteroids", nullptr, false, object))
				RandomizeAsteroids();
			if(ImGui::MenuItem("Randomize Minables", nullptr, false, object))
				RandomizeMinables();
			ImGui::Separator();
			if(ImGui::MenuItem("Randomize All", nullptr, false, object))
			{
				RandomizeAsteroids();
				RandomizeMinables();
				Randomize();
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Generate Trades", nullptr, false, object))
				GenerateTrades();
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showRenameSystem)
		ImGui::OpenPopup("Rename System");
	if(showCloneSystem)
		ImGui::OpenPopup("Clone System");
	AlwaysRender(showNewSystem);
	ImGui::BeginSimpleRenameModal("Rename System", [this](const string &name)
			{
				DeleteFromChanges();
				editor.RenameObject(keyFor<System>(), object->name, name);
				GameData::Systems().Rename(object->name, name);
				object->name = name;
				WriteToPlugin(object, false);
				UpdateMap();
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone System", [this](const string &name)
			{
				auto *clone = const_cast<System *>(GameData::Systems().Get(name));
				*clone = *object;
				object = clone;

				object->name = name;
				object->position += Point(25., 25.);
				object->objects.clear();
				object->links.clear();
				object->attributes.insert("uninhabited");
				editor.Player().Seen(*object);
				GameData::UpdateSystem(object);
				for(auto &&link : object->VisibleNeighbors())
					GameData::UpdateSystem(const_cast<System *>(link));
				UpdateMap();
				SetDirty();
				if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
					panel->Select(object);
				if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
					panel->Select(object);
			});

	if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
		object = const_cast<System *>(panel->Selected());
	if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
		object = const_cast<System *>(panel->Selected());

	if(ImGui::InputCombo("system", &searchBox, &object, GameData::Systems()))
	{
		searchBox.clear();
		if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
			panel->Select(object);
		if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
			panel->Select(object);
	}

	ImGui::Separator();
	ImGui::Spacing();
	if(object)
		RenderSystem();
	ImGui::End();
}



void SystemEditor::AlwaysRender(bool showNewSystem)
{
	if(showNewSystem || createNewSystem)
		ImGui::OpenPopup("New System");
	ImGui::BeginSimpleNewModal("New System", [this](const string &name)
			{
				auto *newSystem = const_cast<System *>(GameData::Systems().Get(name));

				newSystem->name = name;
				newSystem->position = createNewSystem ? position : object->position + Point(25., 25.);
				newSystem->attributes.insert("uninhabited");
				newSystem->isDefined = true;
				newSystem->hasPosition = true;
				editor.Player().Seen(*newSystem);
				object = newSystem;
				GameData::UpdateSystem(object);
				for(auto &&link : object->VisibleNeighbors())
					GameData::UpdateSystem(const_cast<System *>(link));
				UpdateMap();
				SetDirty();
				if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
					panel->Select(object);
				if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
					panel->Select(object);
			});
	createNewSystem &= ImGui::IsPopupOpen("New System");
}



void SystemEditor::RenderSystem()
{
	int index = 0;

	ImGui::Text("name: %s", object->name.c_str());
	if(ImGui::Checkbox("hidden", &object->hidden))
		SetDirty();

	if(ImGui::TreeNode("attributes"))
	{
		set<string> toAdd;
		set<string> toRemove;
		for(auto &attribute : object->attributes)
		{
			if(attribute == "uninhabited")
				continue;

			ImGui::PushID(index++);
			string str = attribute;
			if(ImGui::InputText("", &str, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(!str.empty())
					toAdd.insert(move(str));
				toRemove.insert(attribute);
			}
			ImGui::PopID();
		}
		for(auto &&attribute : toAdd)
			object->attributes.insert(attribute);
		for(auto &&attribute : toRemove)
			object->attributes.erase(attribute);
		if(!toAdd.empty() || !toRemove.empty())
			SetDirty();

		ImGui::Spacing();

		static string addAttribute;
		if(ImGui::InputText("##system", &addAttribute, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->attributes.insert(move(addAttribute));
			SetDirty();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("links"))
	{
		set<System *> toAdd;
		set<System *> toRemove;
		index = 0;
		for(auto &link : object->links)
		{
			ImGui::PushID(index++);
			static System *newLink = nullptr;
			string name = link->Name();
			if(ImGui::InputCombo("link", &name, &newLink, GameData::Systems()))
			{
				if(newLink)
					toAdd.insert(newLink);
				newLink = nullptr;
				toRemove.insert(const_cast<System *>(link));
			}
			ImGui::PopID();
		}
		static System *newLink = nullptr;
		string addLink;
		if(ImGui::InputCombo("add link", &addLink, &newLink, GameData::Systems()))
			if(newLink)
			{
				toAdd.insert(newLink);
				newLink = nullptr;
			}

		if(toAdd.count(object))
		{
			toAdd.erase(object);
			toRemove.erase(object);
		}
		for(auto &sys : toAdd)
		{
			object->Link(sys);
			GameData::UpdateSystem(sys);
			SetDirty(sys);
		}
		for(auto &&sys : toRemove)
		{
			object->Unlink(sys);
			GameData::UpdateSystem(sys);
			SetDirty(sys);
		}
		if(!toAdd.empty() || !toRemove.empty())
		{
			GameData::UpdateSystem(object);
			SetDirty();
			UpdateMap();
		}
		ImGui::TreePop();
	}

	bool asteroidsOpen = ImGui::TreeNode("asteroids");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Asteroid"))
		{
			object->asteroids.emplace_back("small rock", 1, 1.);
			UpdateMain();
			SetDirty();
		}
		if(ImGui::Selectable("Add Mineable"))
		{
			object->asteroids.emplace_back(&GameData::Minables().begin()->second, 1, 1.);
			UpdateMain();
			SetDirty();
		}
		ImGui::EndPopup();
	}

	if(asteroidsOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &asteroid : object->asteroids)
		{
			ImGui::PushID(index);
			if(asteroid.Type())
			{
				bool open = ImGui::TreeNode("minables", "mineables: %s %d %g", asteroid.Type()->Name().c_str(), asteroid.count, asteroid.energy);
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
						toRemove = index;
					ImGui::EndPopup();
				}

				if(open)
				{
					if(ImGui::BeginCombo("name", asteroid.Type()->Name().c_str()))
					{
						int index = 0;
						for(const auto &item : GameData::Minables())
						{
							const bool selected = &item.second == asteroid.Type();
							if(ImGui::Selectable(item.first.c_str(), selected))
							{
								asteroid.type = &item.second;
								UpdateMain();
								SetDirty();
							}
							++index;

							if(selected)
								ImGui::SetItemDefaultFocus();
						}
						ImGui::EndCombo();
					}
					if(ImGui::InputInt("count", &asteroid.count))
					{
						UpdateMain();
						SetDirty();
					}
					if(ImGui::InputDoubleEx("energy", &asteroid.energy))
					{
						UpdateMain();
						SetDirty();
					}
					ImGui::TreePop();
				}
			}
			else
			{
				bool open = ImGui::TreeNode("asteroids", "asteroids: %s %d %g", asteroid.name.c_str(), asteroid.count, asteroid.energy);
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
					{
						UpdateMain();
						toRemove = index;
					}
					ImGui::EndPopup();
				}

				if(open)
				{
					if(ImGui::InputText("name", &asteroid.name))
					{
						UpdateMain();
						SetDirty();
					}
					if(ImGui::InputInt("count", &asteroid.count))
					{
						UpdateMain();
						SetDirty();
					}
					if(ImGui::InputDoubleEx("energy", &asteroid.energy))
					{
						UpdateMain();
						SetDirty();
					}
					ImGui::TreePop();
				}
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			object->asteroids.erase(object->asteroids.begin() + toRemove);
			UpdateMain();
			SetDirty();
		}
		ImGui::TreePop();
	}

	bool fleetOpen = ImGui::TreeNode("fleets");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Fleet"))
			object->fleets.emplace_back(&GameData::Fleets().begin()->second, 1);
		ImGui::EndPopup();
	}

	if(fleetOpen)
	{
		index = 0;
		int toRemove = -1;
		for(auto &fleet : object->fleets)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("fleet", "fleet: %s %d", fleet.Get()->Name().c_str(), fleet.period);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}

			if(open)
			{
				static Fleet *selected;
				string fleetName = fleet.Get() ? fleet.Get()->Name() : "";
				if(ImGui::InputCombo("fleet", &fleetName, &selected, GameData::Fleets()))
					if(selected)
					{
						fleet.event = selected;
						selected = nullptr;
						SetDirty();
					}
				if(ImGui::InputInt("period", &fleet.period))
					SetDirty();
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}
		if(toRemove != -1)
		{
			object->fleets.erase(object->fleets.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}

	RenderHazards(object->hazards);

	double pos[2] = {object->position.X(), object->Position().Y()};
	if(ImGui::InputDouble2Ex("pos", pos))
	{
		object->position.Set(pos[0], pos[1]);
		SetDirty();
	}

	{
		static Government *selected;
		string govName = object->government ? object->government->TrueName() : "";
		if(ImGui::InputCombo("government", &govName, &selected, GameData::Governments()))
		{
			object->government = selected;
			UpdateMap();
			SetDirty();
		}
	}

	if(ImGui::InputText("music", &object->music))
		SetDirty();

	if(ImGui::InputDoubleEx("habitable", &object->habitable))
		SetDirty();
	if(ImGui::InputDoubleEx("belt", &object->asteroidBelt))
		SetDirty();
	if(ImGui::InputDoubleEx("jump range", &object->jumpRange))
	{
		if(auto *panel = dynamic_cast<MapEditorPanel *>(editor.GetMenu().Top().get()))
			panel->UpdateJumpDistance();
		GameData::UpdateSystem(const_cast<System *>(object));
		SetDirty();
	}
	if(object->jumpRange < 0.)
		object->jumpRange = 0.;
	string enterHaze = object->haze ? object->haze->Name() : "";
	if(ImGui::InputCombo("haze", &enterHaze, &object->haze, SpriteSet::GetSprites()))
	{
		enterHaze = object->haze->Name();
		UpdateMain();
		SetDirty();
	}

	double arrival[2] = {object->extraHyperArrivalDistance, object->extraJumpArrivalDistance};
	if(ImGui::InputDouble2Ex("arrival", arrival))
		SetDirty();
	object->extraHyperArrivalDistance = arrival[0];
	object->extraJumpArrivalDistance = fabs(arrival[1]);

	if(ImGui::TreeNode("trades"))
	{
		index = 0;
		for(auto &&commodity : GameData::Commodities())
		{
			ImGui::PushID(index++);
			ImGui::Text("trade: %s", commodity.name.c_str());
			ImGui::SameLine();
			int value = 0;
			auto it = object->trade.find(commodity.name);
			if(it != object->trade.end())
				value = it->second.base;
			if(ImGui::InputInt("", &value))
			{
				if(!value && it != object->trade.end())
					object->trade.erase(it);
				else if(value)
					object->trade[commodity.name].SetBase(value);
				SetDirty();
			}
			ImGui::PopID();
		}
		ImGui::TreePop();
	}

	bool openObjects = ImGui::TreeNode("objects");
	bool openAddObject = false;
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Object"))
			openAddObject = true;
		ImGui::EndPopup();
	}
	if(openAddObject)
	{
		object->objects.emplace_back();
		SetDirty();
	}

	if(openObjects)
	{
		bool hovered = false;
		bool add = false;
		index = 0;
		int nested = 0;
		auto selected = object->objects.end();
		auto selectedToAdd = object->objects.end();
		for(auto it = object->objects.begin(); it != object->objects.end(); ++it)
		{
			ImGui::PushID(index);
			RenderObject(*it, index, nested, hovered, add);
			if(hovered)
			{
				selected = it;
				hovered = false;
			}
			if(add)
			{
				selectedToAdd = it;
				add = false;
			}
			++index;
			ImGui::PopID();
		}
		ImGui::TreePop();

		if(selected != object->objects.end())
		{
			if(auto *planet = selected->GetPlanet())
				const_cast<Planet *>(planet)->RemoveSystem(object);
			SetDirty();
			auto index = selected - object->objects.begin();
			auto next = object->objects.erase(selected);
			size_t removed = 1;
			// Remove any child objects too.
			while(next != object->objects.end() && next->Parent() == index)
			{
				next = object->objects.erase(next);
				++removed;
			}

			// Recalculate every parent index.
			for(auto it = next; it != object->objects.end(); ++it)
				if(it->parent >= index)
					it->parent -= removed;
		}
		else if(selectedToAdd != object->objects.end())
		{
			SetDirty();
			auto it = object->objects.emplace(selectedToAdd + 1);
			it->parent = selectedToAdd - object->objects.begin();

			int newParent = it->parent;
			for(++it; it != object->objects.end(); ++it)
				if(it->parent >= newParent)
					++it->parent;
		}
	}
}



void SystemEditor::RenderObject(StellarObject &object, int index, int &nested, bool &hovered, bool &add)
{
	if(object.parent != -1 && !nested)
		return;

	string selectedString = &object == selectedObject ? "(selected)"
		: selectedObject && selectedObject->parent != -1
			&& &object == &this->object->objects[selectedObject->parent] ? "(child selected)"
		: "";
	string planetString = object.GetPlanet() ? object.GetPlanet()->TrueName() : "";
	if(!planetString.empty() && !selectedString.empty())
		planetString += ' ';
	bool isOpen = ImGui::TreeNode("object", "object %s%s", planetString.c_str(), selectedString.c_str());

	ImGui::PushID(index);
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::MenuItem("Add Child", nullptr, false, object.parent == -1))
			add = true;
		if(ImGui::MenuItem("Remove"))
			hovered = true;
		ImGui::EndPopup();
	}
	ImGui::PopID();

	if(isOpen)
	{
		static Planet *planet = nullptr;
		static string planetName;
		planetName.clear();
		if(object.planet)
			planetName = object.planet->TrueName();
		if(ImGui::InputCombo("planet", &planetName, &planet, GameData::Planets()))
		{
			if(object.planet)
				const_cast<Planet *>(object.planet)->RemoveSystem(this->object);
			object.planet = planet;
			if(planet)
			{
				planet->SetSystem(this->object);
				planet = nullptr;
			}
			SetDirty();
		}
		static Sprite *sprite = nullptr;
		static string spriteName;
		spriteName.clear();
		if(object.sprite)
			spriteName = object.sprite->Name();
		if(ImGui::InputCombo("sprite", &spriteName, &sprite, SpriteSet::GetSprites()))
		{
			object.sprite = sprite;
			sprite = nullptr;
			SetDirty();
		}

		if(ImGui::InputDoubleEx("distance", &object.distance))
			SetDirty();
		double period = 0.;
		if(object.Speed())
			period = 360. / object.Speed();
		if(ImGui::InputDoubleEx("period", &period))
		{
			object.speed = 360. / period;
			SetDirty();
		}
		if(ImGui::InputDoubleEx("offset", &object.offset))
			SetDirty();

		RenderHazards(object.hazards);

		if(index + 1 < static_cast<int>(this->object->objects.size()) && this->object->objects[index + 1].Parent() == index)
		{
			++nested; // If the next object is a child, don't close this tree node just yet.
			return;
		}
		else
			ImGui::TreePop();
	}

	// If are nested, then we need to remove this nesting until we are at the next desired level.
	if(nested && index + 1 >= static_cast<int>(this->object->objects.size()))
		while(nested--)
			ImGui::TreePop();
	else if(nested)
	{
		int nextParent = this->object->objects[index + 1].Parent();
		if(nextParent == object.Parent())
			return;
		while(nextParent != index)
		{
			nextParent = nextParent == -1 ? index : this->object->objects[nextParent].Parent();
			--nested;
			ImGui::TreePop();
		}
	}
}



void SystemEditor::RenderHazards(std::vector<RandomEvent<Hazard>> &hazards)
{
	bool hazardOpen = ImGui::TreeNode("hazards");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add Hazard"))
			hazards.emplace_back(&GameData::Hazards().begin()->second, 1);
		ImGui::EndPopup();
	}

	if(hazardOpen)
	{
		int index = 0;
		int toRemove = -1;
		for(auto &hazard : hazards)
		{
			ImGui::PushID(index);
			bool open = ImGui::TreeNode("hazard", "hazard: %s %d", hazard.Get()->Name().c_str(), hazard.period);
			if(ImGui::BeginPopupContextItem())
			{
				if(ImGui::Selectable("Remove"))
					toRemove = index;
				ImGui::EndPopup();
			}

			if(open)
			{
				static Hazard *selected;
				string hazardName = hazard.Get() ? hazard.Get()->Name() : "";
				if(ImGui::InputCombo("hazard", &hazardName, &selected, GameData::Hazards()))
					if(selected)
					{
						hazard.event = selected;
						SetDirty();
					}
				if(ImGui::InputInt("period", &hazard.period))
					SetDirty();
				ImGui::TreePop();
			}
			++index;
			ImGui::PopID();
		}

		if(toRemove != -1)
		{
			hazards.erase(hazards.begin() + toRemove);
			SetDirty();
		}
		ImGui::TreePop();
	}
}



void SystemEditor::WriteObject(DataWriter &writer, const System *system, const StellarObject *object, bool add)
{
	// Calculate the nesting of this object. We follow parent indices until we reach
	// the root node.
	int i = object->Parent();
	int nested = 0;
	while(i != -1)
	{
		i = system->objects[i].Parent();
		++nested;
	}

	for(i = 0; i < nested; ++i)
		writer.BeginChild();

	if(add && !nested)
		writer.WriteToken("add");
	writer.WriteToken("object");

	if(object->GetPlanet())
		writer.WriteToken(object->GetPlanet()->TrueName());
	writer.Write();

	writer.BeginChild();
	if(object->GetSprite())
		writer.Write("sprite", object->GetSprite()->Name());
	if(object->distance)
		writer.Write("distance", object->Distance());
	if(object->speed)
		writer.Write("period", 360. / object->Speed());
	if(object->Offset())
		writer.Write("offset", object->Offset());
	for(const auto &hazard : object->hazards)
		writer.Write("hazard", hazard.Name(), hazard.Period());
	writer.EndChild();

	for(i = 0; i < nested; ++i)
		writer.EndChild();
}



void SystemEditor::WriteToFile(DataWriter &writer, const System *system)
{
	const auto *diff = GameData::baseSystems.Has(system->name)
		? GameData::baseSystems.Get(system->name)
		: nullptr;

	writer.Write("system", system->name);
	writer.BeginChild();

	if((!diff && system->hasPosition) || (diff && (system->hasPosition != diff->hasPosition || system->position != diff->position)))
		writer.Write("pos", system->position.X(), system->position.Y());
	if(!diff || system->government != diff->government)
	{
		if(system->government)
			writer.Write("government", system->government->TrueName());
		else if(diff)
			writer.Write("remove", "government");
	}
	if(!diff || system->music != diff->music)
	{
		if(!system->music.empty())
			writer.Write("music", system->music);
		else if(diff)
			writer.Write("remove", "music");
	}
	WriteDiff(writer, "link", system->links, diff ? &diff->links : nullptr);
	if(!diff || system->hidden != diff->hidden)
	{
		if(system->hidden)
			writer.Write("hidden");
		else if(diff)
			writer.Write("remove", "hidden");
	}

	auto asteroids = system->asteroids;
	asteroids.erase(std::remove_if(asteroids.begin(), asteroids.end(), [](const System::Asteroid &a) { return a.Type(); }), asteroids.end());
	auto minables = system->asteroids;
	minables.erase(std::remove_if(minables.begin(), minables.end(), [](const System::Asteroid &a) { return !a.Type(); }), minables.end());
	auto diffAsteroids = diff ? diff->asteroids : system->asteroids;
	diffAsteroids.erase(std::remove_if(diffAsteroids.begin(), diffAsteroids.end(), [](const System::Asteroid &a) { return a.Type(); }), diffAsteroids.end());
	auto diffMinables = diff ? diff->asteroids : system->asteroids;
	diffMinables.erase(std::remove_if(diffMinables.begin(), diffMinables.end(), [](const System::Asteroid &a) { return !a.Type(); }), diffMinables.end());
	WriteDiff(writer, "asteroids", asteroids, diff ? &diffAsteroids : nullptr);
	WriteDiff(writer, "minables", minables, diff ? &diffMinables : nullptr);

	if(!diff || system->haze != diff->haze)
	{
		if(system->haze)
			writer.Write("haze", system->haze->Name());
		else if(diff)
			writer.Write("remove", "haze");
	}
	WriteDiff(writer, "fleet", system->fleets, diff ? &diff->fleets : nullptr);
	WriteDiff(writer, "hazard", system->hazards, diff ? &diff->hazards : nullptr);
	if((!diff && system->habitable != 1000.) || (diff && system->habitable != diff->habitable))
		writer.Write("habitable", system->habitable);
	if((!diff && system->asteroidBelt != 1500.) || (diff && system->asteroidBelt != diff->asteroidBelt))
		writer.Write("belt", system->asteroidBelt);
	if((!diff && system->jumpRange)|| (diff && system->jumpRange != diff->jumpRange))
		writer.Write("jump range", system->jumpRange);
	if(!diff || system->extraHyperArrivalDistance != diff->extraHyperArrivalDistance
			|| system->extraJumpArrivalDistance != diff->extraJumpArrivalDistance)
	{
		if(system->extraHyperArrivalDistance == system->extraJumpArrivalDistance
				&& (diff || system->extraHyperArrivalDistance))
			writer.Write("arrival", system->extraHyperArrivalDistance);
		else if(system->extraHyperArrivalDistance != system->extraJumpArrivalDistance)
		{
			writer.Write("arrival");
			writer.BeginChild();
			if((!diff && system->extraHyperArrivalDistance) || system->extraHyperArrivalDistance != diff->extraHyperArrivalDistance)
				writer.Write("link", system->extraHyperArrivalDistance);
			if((!diff && system->extraJumpArrivalDistance) || system->extraJumpArrivalDistance != diff->extraJumpArrivalDistance)
				writer.Write("jump", system->extraJumpArrivalDistance);
			writer.EndChild();
		}
	}
	if(!diff || system->trade != diff->trade)
	{
		auto trades = system->trade;
		if(diff)
		{
			bool hasRemoved = false;
			for(auto it = diff->trade.begin(); it != diff->trade.end(); ++it)
				if(system->trade.find(it->first) == system->trade.end())
				{
					writer.Write("remove", "trade");
					hasRemoved = true;
					break;
				}
			if(!hasRemoved)
				for(auto it = trades.begin(); it != trades.end();)
				{
					auto jt = diff->trade.find(it->first);
					if(jt != diff->trade.end() && jt->second == it->second)
						it = trades.erase(it);
					else
						++it;
				}
		}

		if(!trades.empty())
			for(auto &&trade : trades)
				writer.Write("trade", trade.first, trade.second.base);
	}

	auto systemAttributes = system->attributes;
	auto diffAttributes = diff ? diff->attributes : system->attributes;
	systemAttributes.erase("uninhabited");
	diffAttributes.erase("uninhabited");
	WriteDiff(writer, "attributes", systemAttributes, diff ? &diffAttributes : nullptr, true);

	if(!diff || system->objects != diff->objects)
	{
		std::vector<const StellarObject *> toAdd;
		if(diff && system->objects.size() > diff->objects.size())
		{
			std::transform(system->objects.begin() + diff->objects.size(), system->objects.end(),
					back_inserter(toAdd), [](const StellarObject &obj) { return &obj; });

			for(int i = 0; i < static_cast<int>(diff->objects.size()); ++i)
				if(system->objects[i] != diff->objects[i])
				{
					toAdd.clear();
					break;
				}
		}

		if(!toAdd.empty())
			for(auto &&object : toAdd)
				WriteObject(writer, system, object, true);
		else if(!system->objects.empty())
			for(auto &&object : system->objects)
				WriteObject(writer, system, &object);
		else if(diff)
			writer.Write("remove object");
	}

	writer.EndChild();
}



void SystemEditor::SaveCurrent()
{
	if(object && editor.HasPlugin() && IsDirty())
		WriteToPlugin(object);
}



void SystemEditor::Delete(const std::vector<const System *> &systems)
{
	for(const auto &system : systems)
		Delete(system, false);
}



void SystemEditor::UpdateMap() const
{
	if(auto *mapPanel = dynamic_cast<MapPanel*>(editor.GetUI().Top().get()))
	{
		mapPanel->UpdateCache();
		mapPanel->distance = DistanceMap(editor.Player());
	}
	if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
		panel->UpdateCache();
}



void SystemEditor::UpdateMain() const
{
	if(auto *panel = dynamic_cast<MainEditorPanel *>(editor.GetMenu().Top().get()))
		panel->UpdateCache();
}



void SystemEditor::Randomize()
{
	// Randomizes a star system.
	// Code adapted from the official ES map editor under GPL 3.0.
	// https://github.com/endless-sky/endless-sky-editor
	constexpr int STAR_DISTANCE = 40;
	static random_device rd;
	static mt19937 gen(rd());
	static uniform_int_distribution<> randStarNum(0, 2);
	static uniform_int_distribution<> randStarDist(0, STAR_DISTANCE);

	auto getRadius = [](const StellarObject &stellar)
	{
		return stellar.sprite->Width() / 2. - 4.;
	};
	auto getMass = [](const StellarObject &stellar)
	{
		constexpr double STAR_MASS_SCALE = .25;
		const auto radius = stellar.Radius();
		return radius * radius * STAR_MASS_SCALE;
	};

	object->objects.clear();

	// First we generate the (or 2) star(s).
	const int numStars = 1 + !randStarNum(gen);
	double mass;
	if(numStars == 1)
	{
		StellarObject stellar;
		stellar.sprite = RandomStarSprite();
		stellar.speed = 36.;
		mass = getMass(stellar);

		object->objects.push_back(stellar);
	}
	else
	{
		StellarObject stellar1;
		StellarObject stellar2;

		stellar1.isStar = true;
		stellar2.isStar = true;

		stellar1.sprite = RandomStarSprite();
		do stellar2.sprite = RandomStarSprite();
		while (stellar2.sprite == stellar1.sprite);

		stellar2.offset = 180.;

		double radius1 = getRadius(stellar1);
		double radius2 = getRadius(stellar2);

		// Make sure the 2 stars have similar radius.
		while(fabs(radius1 - radius2) > 100.)
		{
			do stellar1.sprite = RandomStarSprite();
			while (stellar1.sprite == stellar2.sprite);
			radius1 = getRadius(stellar1);
		}

		double mass1 = getMass(stellar1);
		double mass2 = getMass(stellar2);
		mass = mass1 + mass2;

		double distance = radius1 + radius2 + randStarDist(gen) + STAR_DISTANCE;
		stellar1.distance = (mass2 * distance) / mass;
		stellar2.distance = (mass1 * distance) / mass;

		double period = sqrt(distance * distance * distance / mass);
		stellar1.speed = 360. / period;
		stellar2.speed = 360. / period;

		if(radius1 > radius2)
			swap(stellar1, stellar2);
		object->objects.push_back(stellar1);
		object->objects.push_back(stellar2);
	}

	constexpr double HABITABLE_SCALE = 1.25;
	object->habitable = mass / HABITABLE_SCALE;

	// Now we generate lots of planets with moons.
	uniform_int_distribution<> randPlanetCount(4, 6);
	int planetCount = randPlanetCount(gen);
	for(int i = 0; i < planetCount; ++i)
	{
		constexpr int RANDOM_SPACE = 100;
		int space = RANDOM_SPACE;
		for(const auto &stellar : object->objects)
			if(!stellar.isStar && stellar.parent == -1)
				space += RANDOM_SPACE / 2;

		int distance = object->objects.back().distance;
		if(object->objects.back().sprite)
			distance += getRadius(object->objects.back());
		if(object->objects.back().parent != -1)
			distance += object->objects[object->objects.back().parent].distance;

		uniform_int_distribution<> randSpace(0, space);
		const int addSpace = randSpace(gen);
		distance += (addSpace * addSpace) * .01 + 50.;

		set<const Sprite *> used;
		for(const auto &stellar : object->objects)
			used.insert(stellar.sprite);

		uniform_int_distribution<> rand10(0, 9);
		uniform_int_distribution<> rand2000(0, 1999);
		const bool isHabitable = distance > object->habitable * .5 && distance < object->habitable * 2.;
		const bool isSmall = !rand10(gen);
		const bool isTerrestrial = !isSmall && rand2000(gen) > distance;

		const int rootIndex = static_cast<int>(object->objects.size());

		const Sprite *planetSprite;
		do {
			if(isSmall)
				planetSprite = RandomMoonSprite();
			else if(isTerrestrial)
				planetSprite = isHabitable ? RandomPlanetSprite() : RandomPlanetSprite();
			else
				planetSprite = RandomGiantSprite();
		} while(used.count(planetSprite));

		object->objects.emplace_back();
		object->objects.back().sprite = planetSprite;
		object->objects.back().isStation = !planetSprite->Name().compare(0, 14, "planet/station");
		used.insert(planetSprite);
		auto &planet = object->objects.back();

		uniform_int_distribution<> oneOrTwo(1, 2);
		uniform_int_distribution<> threeOrFive(3, 5);

		const int randMoon = isTerrestrial ? oneOrTwo(gen) : threeOrFive(gen);
		int moonCount = uniform_int_distribution<>(0, randMoon - 1)(gen);
		if(getRadius(object->objects.back()) < 70.)
			moonCount = 0;

		auto calcPeriod = [this, &getRadius](StellarObject &stellar, bool isMoon)
		{
			const auto radius = getRadius(stellar);
			constexpr double PLANET_MASS_SCALE = .015;
			const auto mass = isMoon ? radius * radius * radius * PLANET_MASS_SCALE
				: object->habitable * HABITABLE_SCALE;

			stellar.speed = 360. / sqrt(stellar.distance * stellar.distance * stellar.distance / mass);
		};

		double moonDistance = getRadius(object->objects.back());
		int randomMoonSpace = 50.;
		for(int i = 0; i < moonCount; ++i)
		{
			uniform_int_distribution<> randMoonDist(10., randomMoonSpace);
			moonDistance += randMoonDist(gen);
			randomMoonSpace += 20.;

			const Sprite *moonSprite;
			do moonSprite = RandomMoonSprite();
			while(used.count(moonSprite));
			used.insert(moonSprite);

			object->objects.emplace_back();
			object->objects.back().isMoon = true;
			object->objects.back().isStation = !moonSprite->Name().compare(0, 14, "planet/station");
			object->objects.back().sprite = moonSprite;
			object->objects.back().parent = rootIndex;
			object->objects.back().distance = moonDistance + getRadius(object->objects.back());
			calcPeriod(object->objects.back(), true);
			moonDistance += 2. * getRadius(object->objects.back());
		}

		planet.distance = distance + moonDistance;
		calcPeriod(planet, false);
	}

	SetDirty();
}



void SystemEditor::RandomizeAsteroids()
{
	// Randomizes the asteroids in this system.
	// Code adapted from the official ES map editor under GPL 3.0.
	// https://github.com/endless-sky/endless-sky-editor
	object->asteroids.erase(remove_if(object->asteroids.begin(), object->asteroids.end(),
				[](const System::Asteroid &asteroid) { return !asteroid.Type(); }),
			object->asteroids.end());

	static random_device rd;
	static mt19937 gen(rd());

	uniform_int_distribution<> rand(0, 21);
	const int total = rand(gen) * rand(gen) + 1;
	const double energy = (rand(gen) + 10) * (rand(gen) + 10) * .01;
	const string prefix[] = { "small", "medium", "large" };
	const char *suffix[] = { " rock", " metal" };

	uniform_int_distribution<> randCount(0, total - 1);
	int amount[] = { randCount(gen), 0 };
	amount[1] = total - amount[0];

	for(int i = 0; i < 2; ++i)
	{
		if(!amount[i])
			continue;

		uniform_int_distribution<> randCount(0, amount[i] - 1);
		int count[] = { 0, randCount(gen), 0 };
		int remaining = amount[i] - count[1];
		if(remaining)
		{
			uniform_int_distribution<> randRemaining(0, remaining - 1);
			count[0] = randRemaining(gen);
			count[2] = remaining - count[0];
		}

		for(int j = 0; j < 3; ++j)
			if(count[j])
			{
				uniform_int_distribution<> randEnergy(50, 100);
				object->asteroids.emplace_back(prefix[j] + suffix[i], count[j], energy * randEnergy(gen) * .01);
			}
	}

	UpdateMain();
	SetDirty();
}



void SystemEditor::RandomizeMinables()
{
	// Randomizes the minables in this system.
	// Code adapted from the official ES map editor under GPL 3.0.
	// https://github.com/endless-sky/endless-sky-editor
	object->asteroids.erase(remove_if(object->asteroids.begin(), object->asteroids.end(),
				[](const System::Asteroid &asteroid) { return asteroid.Type(); }),
			object->asteroids.end());

	static random_device rd;
	static mt19937 gen(rd());

	uniform_int_distribution<> randBelt(1000, 2000);
	object->asteroidBelt = randBelt(gen);

	int totalCount = 0;
	double totalEnergy = 0.;
	for(const auto &asteroid : object->asteroids)
	{
		totalCount += asteroid.count;
		totalEnergy += asteroid.energy * asteroid.count;
	}

	if(!totalCount)
	{
		// This system has no other asteroids, so we generate a few minables only.
		totalCount = 1;
		uniform_real_distribution<> randEnergy(50., 100.);
		totalEnergy = randEnergy(gen) * .01;
	}

	double meanEnergy = totalEnergy / totalCount;
	totalCount /= 4;

	const unordered_map<const char *, double> probabilities = {
        { "aluminum", 12 },
        { "copper", 8 },
        { "gold", 2 },
        { "iron", 13 },
        { "lead", 15 },
        { "neodymium", 3 },
        { "platinum", 1 },
        { "silicon", 2 },
        { "silver", 5 },
        { "titanium", 11 },
        { "tungsten", 6 },
        { "uranium", 4 },
	};

	unordered_map<const char *, int> choices;
	uniform_int_distribution<> rand100(0, 99);
	for(int i = 0; i < 3; ++i)
	{
		uniform_int_distribution<> randCount(0, totalCount);
		totalCount = randCount(gen);
		if(!totalCount)
			break;

		int choice = rand100(gen);
		for(const auto &pair : probabilities)
		{
			choice -= pair.second;
			if(choice < 0)
			{
				choices[pair.first] += totalCount;
				break;
			}
		}
	}

	for(const auto &pair : choices)
	{
		const double energy = randBelt(gen) * .001 * meanEnergy;
		object->asteroids.emplace_back(GameData::Minables().Get(pair.first), pair.second, energy);
	}

	UpdateMain();
	SetDirty();
}



void SystemEditor::GenerateTrades()
{
	static random_device rd;
	static mt19937 gen(rd());
	for(const auto &commodity : GameData::Commodities())
	{
		int average = 0;
		int size = 0;
		for(const auto &link : object->links)
		{
			auto it = link->trade.find(commodity.name);
			if(it != link->trade.end())
			{
				average += it->second.base;
				++size;
			}
		}
		if(size)
			average /= size;

		// This system doesn't have any neighbors with the given commodity so we generate
		// a random price for it.
		if(!average)
		{
			uniform_int_distribution<> rand(commodity.low, commodity.high);
			average = rand(gen);
		}
		const int maxDeviation = (commodity.high - commodity.low) / 8;
		uniform_int_distribution<> rand(
				max(commodity.low, average - maxDeviation),
				min(commodity.high, average + maxDeviation));
		object->trade[commodity.name].SetBase(rand(gen));
	}

	SetDirty();
}



const Sprite *SystemEditor::RandomStarSprite()
{
	static random_device rd;
	static mt19937 gen(rd());

	const auto &stars = GameData::Stars();
	uniform_int_distribution<> randStar(0, stars.size() - 1);
	return stars[randStar(gen)];
}



const Sprite *SystemEditor::RandomPlanetSprite(bool recalculate)
{
	static random_device rd;
	static mt19937 gen(rd());

	const auto &planets = GameData::PlanetSprites();
	uniform_int_distribution<> randPlanet(0, planets.size() - 1);
	return planets[randPlanet(gen)];
}



const Sprite *SystemEditor::RandomMoonSprite()
{
	static random_device rd;
	static mt19937 gen(rd());

	const auto &moons = GameData::MoonSprites();
	uniform_int_distribution<> randMoon(0, moons.size() - 1);
	return moons[randMoon(gen)];
}



const Sprite *SystemEditor::RandomGiantSprite()
{
	static random_device rd;
	static mt19937 gen(rd());

	const auto &giants = GameData::GiantSprites();
	uniform_int_distribution<> randGiant(0, giants.size() - 1);
	return giants[randGiant(gen)];
}



void SystemEditor::Delete(const System *system, bool safe)
{
	if(safe || !GameData::baseSystems.Has(system->name))
	{
		if(find_if(Changes().begin(), Changes().end(), [&system](const System &sys)
					{
						return sys.name == system->name;
					}) != Changes().end())
		{
			SetDirty("[deleted]");
			DeleteFromChanges();
		}
		else
			SetClean();
		auto oldLinks = system->links;
		for(auto &&link : oldLinks)
		{
			const_cast<System *>(link)->Unlink(const_cast<System *>(system));
			SetDirty(link);
			GameData::UpdateSystem(const_cast<System *>(link));
		}
		for(auto &&stellar : system->Objects())
			if(stellar.planet)
				const_cast<Planet *>(stellar.planet)->RemoveSystem(system);

		auto oldNeighbors = system->VisibleNeighbors();
		GameData::Systems().Erase(system->name);
		for(auto &&link : oldNeighbors)
			GameData::UpdateSystem(const_cast<System *>(link));

		auto newSystem = oldLinks.empty() ?
			oldNeighbors.empty() ? nullptr : *oldNeighbors.begin()
			: *oldLinks.begin();
		if(auto *panel = dynamic_cast<MapEditorPanel*>(editor.GetMenu().Top().get()))
			panel->Select(newSystem);
		if(auto *panel = dynamic_cast<MainEditorPanel*>(editor.GetMenu().Top().get()))
			panel->Select(newSystem);
		UpdateMap();
	}
}
