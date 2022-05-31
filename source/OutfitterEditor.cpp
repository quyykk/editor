/* OutfitterEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "OutfitterEditor.h"

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



OutfitterEditor::OutfitterEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Sale<Outfit>>(editor, show)
{
}



void OutfitterEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Outfitter Editor", &show, ImGuiWindowFlags_MenuBar))
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

	bool showNewOutfitter = false;
	bool showRenameOutfitter = false;
	bool showCloneOutfitter = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Outfitter"))
		{
			const bool alreadyDefined = object && !GameData::baseOutfitSales.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewOutfitter);
			ImGui::MenuItem("Rename", nullptr, &showRenameOutfitter, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneOutfitter, object);
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

				if(!found && GameData::baseOutfitSales.Has(object->name))
					*object = *GameData::baseOutfitSales.Get(object->name);
				else if(!found)
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
					GameData::Outfitters().Erase(object->name);
					object = nullptr;
				}
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				if(find_if(Changes().begin(), Changes().end(), [this](const Sale<Outfit> &outfitter)
							{
								return outfitter.name == object->name;
							}) != Changes().end())
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
				}
				else
					SetClean();
				GameData::Outfitters().Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewOutfitter)
		ImGui::OpenPopup("New Outfitter");
	if(showRenameOutfitter)
		ImGui::OpenPopup("Rename Outfitter");
	if(showCloneOutfitter)
		ImGui::OpenPopup("Clone Outfitter");
	ImGui::BeginSimpleNewModal("New Outfitter", [this](const string &name)
			{
				if(GameData::Outfitters().Find(name))
					return;

				auto *newOutfitter = const_cast<Sale<Outfit> *>(GameData::Outfitters().Get(name));
				newOutfitter->name = name;
				object = newOutfitter;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Outfitter", [this](const string &name)
			{
				if(GameData::Outfitters().Find(name))
					return;

				DeleteFromChanges();
				editor.RenameObject(keyFor<Sale<Outfit>>(), object->name, name);
				GameData::Outfitters().Rename(object->name, name);
				object->name = name;
				WriteToPlugin(object, false);
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Outfitter", [this](const string &name)
			{
				if(GameData::Outfitters().Find(name))
					return;

				auto *clone = const_cast<Sale<Outfit> *>(GameData::Outfitters().Get(name));
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});

	if(ImGui::InputCombo("outfitter", &searchBox, &object, GameData::Outfitters()))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	if(object)
		RenderOutfitter();
	ImGui::End();
}



void OutfitterEditor::RenderOutfitter()
{
	ImGui::Text("outfitter: %s", object->name.c_str());
	int index = 0;
	const Outfit *toAdd = nullptr;
	const Outfit *toRemove = nullptr;
	for(auto it = object->begin(); it != object->end(); ++it)
	{
		ImGui::PushID(index++);
		string name = (*it)->Name();
		Outfit *change = nullptr;
		if(ImGui::InputCombo("outfit", &name, &change, GameData::Outfits()))
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

	static string outfitName;
	static Outfit *outfit = nullptr;
	if(ImGui::InputCombo("add outfit", &outfitName, &outfit, GameData::Outfits()))
		if(!outfitName.empty())
		{
			object->insert(outfit);
			SetDirty();
			outfitName.clear();
		}
}



void OutfitterEditor::WriteToFile(DataWriter &writer, const Sale<Outfit> *outfitter)
{
	const auto *diff = GameData::baseOutfitSales.Has(outfitter->name)
		? GameData::baseOutfitSales.Get(outfitter->name)
		: nullptr;

	writer.Write("outfitter", outfitter->name);
	writer.BeginChild();
	if(diff)
		WriteSorted(diff->AsBase(), [](const Outfit *lhs, const Outfit *rhs) { return lhs->Name() < rhs->Name(); },
				[&writer, &outfitter](const Outfit &outfit)
				{
					if(!outfitter->Has(&outfit))
						writer.Write("remove", outfit.Name());
				});
	WriteSorted(outfitter->AsBase(), [](const Outfit *lhs, const Outfit *rhs) { return lhs->Name() < rhs->Name(); },
			[&writer, &diff](const Outfit &outfit)
			{
				if(!diff || !diff->Has(&outfit))
					writer.Write(outfit.Name());
			});
	writer.EndChild();
}
