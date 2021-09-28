/* GalaxyEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "GalaxyEditor.h"

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
#include "Galaxy.h"
#include "GameData.h"
#include "Government.h"
#include "MainPanel.h"
#include "MapEditorPanel.h"
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
#include "gl_header.h"

#include <cassert>
#include <map>

using namespace std;



GalaxyEditor::GalaxyEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Galaxy>(editor, show)
{
}



void GalaxyEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Galaxy Editor", &show, ImGuiWindowFlags_MenuBar))
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

	bool showNewGalaxy = false;
	bool showRenameGalaxy = false;
	bool showCloneGalaxy = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Galaxy"))
		{
			const bool alreadyDefined = object && !GameData::baseGalaxies.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewGalaxy);
			ImGui::MenuItem("Rename", nullptr, &showRenameGalaxy, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneGalaxy, object);
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

				if(!found && GameData::baseGalaxies.Has(object->name))
					*object = *GameData::baseGalaxies.Get(object->name);
				else if(!found)
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
					GameData::Galaxies().Erase(object->name);
					object = nullptr;
				}
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				if(find_if(Changes().begin(), Changes().end(), [this](const Galaxy &galaxy)
							{
								return galaxy.name == object->name;
							}) != Changes().end())
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
				}
				else
					SetClean();
				GameData::Galaxies().Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Go to Galaxy", nullptr, false, object))
				if(auto *panel = dynamic_cast<MapEditorPanel *>(editor.GetMenu().Top().get()))
					panel->Select(object);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewGalaxy)
		ImGui::OpenPopup("New Galaxy");
	if(showRenameGalaxy)
		ImGui::OpenPopup("Rename Galaxy");
	if(showCloneGalaxy)
		ImGui::OpenPopup("Clone Galaxy");
	ImGui::BeginSimpleNewModal("New Galaxy", [this](const string &name)
			{
				auto *newGalaxy = const_cast<Galaxy *>(GameData::Galaxies().Get(name));
				newGalaxy->name = name;
				object = newGalaxy;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Galaxy", [this](const string &name)
			{
				DeleteFromChanges();
				editor.RenameObject(keyFor<Galaxy>(), object->name, name);
				GameData::Galaxies().Rename(object->name, name);
				object->name = name;
				WriteToPlugin(object, false);
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Galaxy", [this](const string &name)
			{
				auto *clone = const_cast<Galaxy *>(GameData::Galaxies().Get(name));
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});

	if(ImGui::InputCombo("galaxy", &searchBox, &object, GameData::Galaxies()))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	if(object)
		RenderGalaxy();
	ImGui::End();
}



void GalaxyEditor::RenderGalaxy()
{
	ImGui::Text("name: %s", object->name.c_str());

	double pos[2] = {object->position.X(), object->Position().Y()};
	if(ImGui::InputDouble2Ex("pos", pos))
	{
		object->position.Set(pos[0], pos[1]);
		SetDirty();
	}

	string spriteName = object->sprite ? object->sprite->Name() : "";
	if(ImGui::InputCombo("sprite", &spriteName, &object->sprite, SpriteSet::GetSprites()))
		SetDirty();

	// FIXME: For some reason the wrong sprite is selected.
	/*if(object->sprite)
	{
		auto id = reinterpret_cast<GLuint>(object->sprite->Texture());
		ImGui::Text("sprite id: %p", reinterpret_cast<void *>(static_cast<intptr_t>(id)));
		glBindTexture(GL_TEXTURE_2D_ARRAY, id);
		ImGui::Image(reinterpret_cast<void *>(static_cast<intptr_t>(id)), ImVec2(1500, 300));
	}*/
}



void GalaxyEditor::WriteToFile(DataWriter &writer, const Galaxy *galaxy)
{
	const auto *diff = GameData::baseGalaxies.Has(galaxy->name)
		? GameData::baseGalaxies.Get(galaxy->name)
		: nullptr;

	writer.Write("galaxy", galaxy->Name());
	writer.BeginChild();

	if(!diff || (diff && (galaxy->position.X() != diff->position.X() || galaxy->position.Y() != diff->position.Y())))
		if(object->position || diff)
			writer.Write("pos", object->position.X(), object->position.Y());
	if(!diff || galaxy->sprite != diff->sprite)
		if(galaxy->sprite)
			writer.Write("sprite", galaxy->sprite->Name());

	writer.EndChild();
}
