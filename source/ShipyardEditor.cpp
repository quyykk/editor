/* ShipyardEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "ShipyardEditor.h"

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
#include "MapPanel.h"
#include "Minable.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Ship.h"
#include "ShipEvent.h"
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"

#include <cassert>
#include <map>

using namespace std;



ShipyardEditor::ShipyardEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Sale<Ship>>(editor, show)
{
}



void ShipyardEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Shipyard Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	bool showNewShipyard = false;
	bool showRenameShipyard = false;
	bool showCloneShipyard = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Shipyard"))
		{
			const bool alreadyDefined = object && !GameData::baseShipSales.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewShipyard);
			ImGui::MenuItem("Rename", nullptr, &showRenameShipyard, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneShipyard, object);
			if(ImGui::MenuItem("Save", nullptr, false, object && editor.HasPlugin() && IsDirty()))
				WriteToPlugin(object);
			if(ImGui::MenuItem("Reset", nullptr, false, object && IsDirty()))
			{
				SetClean();
				bool found = false;
				for(auto &&change : Changes())
					if(change.Name() == object->Name())
					{
						*object = change;
						found = true;
						break;
					}
				if(!found && GameData::baseShipSales.Has(object->name))
					*object = *GameData::baseShipSales.Get(object->name);
				else if(!found)
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
					GameData::Shipyards().Erase(object->name);
					object = nullptr;
				}
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				if(find_if(Changes().begin(), Changes().end(), [this](const Sale<Ship> &shipyard)
							{
								return shipyard.name == object->name;
							}) != Changes().end())
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
				}
				else
					SetClean();
				GameData::Shipyards().Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewShipyard)
		ImGui::OpenPopup("New Shipyard");
	if(showRenameShipyard)
		ImGui::OpenPopup("Rename Shipyard");
	if(showCloneShipyard)
		ImGui::OpenPopup("Clone Shipyard");
	ImGui::BeginSimpleNewModal("New Shipyard", [this](const string &name)
			{
				auto *newShipyard = const_cast<Sale<Ship> *>(GameData::Shipyards().Get(name));
				newShipyard->name = name;
				object = newShipyard;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Shipyard", [this](const string &name)
			{
				DeleteFromChanges();
				editor.RenameObject(keyFor<Sale<Ship>>(), object->name, name);
				GameData::Shipyards().Rename(object->name, name);
				object->name = name;
				WriteToPlugin(object, false);
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Shipyard", [this](const string &name)
			{
				auto *clone = const_cast<Sale<Ship> *>(GameData::Shipyards().Get(name));
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});

	if(ImGui::InputCombo("shipyard", &searchBox, &object, GameData::Shipyards()))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	if(object)
		RenderShipyard();
	ImGui::End();
}



void ShipyardEditor::RenderShipyard()
{
	ImGui::Text("shipyard: %s", object->name.c_str());
	int index = 0;
	const Ship *toAdd = nullptr;
	const Ship *toRemove = nullptr;
	for(auto it = object->begin(); it != object->end(); ++it)
	{
		ImGui::PushID(index++);
		string name = (*it)->TrueName();
		Ship *change = nullptr;
		if(ImGui::InputCombo("ship", &name, &change, GameData::Ships()))
		{
			if(name.empty())
			{
				toRemove = *it;
				SetDirty();
			}
			else
			{
				toAdd = change;
				toRemove = *it;
				SetDirty();
			}
		}
		ImGui::PopID();
	}
	if(toAdd)
		object->insert(toAdd);
	if(toRemove)
		object->erase(toRemove);

	static string shipName;
	static Ship *ship = nullptr;
	if(ImGui::InputCombo("add ship", &shipName, &ship, GameData::Ships()))
		if(!shipName.empty())
		{
			object->insert(ship);
			SetDirty();
			shipName.clear();
		}
}



void ShipyardEditor::WriteToFile(DataWriter &writer, const Sale<Ship> *shipyard)
{
	const auto *diff = GameData::baseShipSales.Has(shipyard->name)
		? GameData::baseShipSales.Get(shipyard->name)
		: nullptr;

	writer.Write("shipyard", shipyard->name);
	writer.BeginChild();
	if(diff)
		WriteSorted(diff->AsBase(), [](const Ship *lhs, const Ship *rhs) { return lhs->TrueName() < rhs->TrueName(); },
				[&writer, &shipyard](const Ship &ship)
				{
					if(!shipyard->Has(&ship))
						writer.Write("remove", ship.TrueName());
				});
	WriteSorted(shipyard->AsBase(), [](const Ship *lhs, const Ship *rhs) { return lhs->TrueName() < rhs->TrueName(); },
			[&writer, &diff](const Ship &ship)
			{
				if(!diff || !diff->Has(&ship))
					writer.Write(ship.TrueName());
			});
	writer.EndChild();
}
