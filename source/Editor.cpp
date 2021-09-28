/* Editor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "Editor.h"

#include "DataFile.h"
#include "DataNode.h"
#include "DataWriter.h"
#include "Dialog.h"
#include "imgui.h"
#include "imgui_ex.h"
#include "imgui_internal.h"
#include "imgui_stdlib.h"
#include "Effect.h"
#include "EsUuid.h"
#include "Engine.h"
#include "Files.h"
#include "GameData.h"
#include "Government.h"
#include "Hazard.h"
#include "ImageSet.h"
#include "MainEditorPanel.h"
#include "MainPanel.h"
#include "MapEditorPanel.h"
#include "MapPanel.h"
#include "Minable.h"
#include "Music.h"
#include "Planet.h"
#include "PlayerInfo.h"
#include "Ship.h"
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"
#include "Visual.h"

#include <cassert>
#include <cstdint>
#include <map>

using namespace std;



Editor::Editor(PlayerInfo &player, UI &menu, UI &ui) noexcept
	: player(player), menu(menu), ui(ui),
	effectEditor(*this, showEffectMenu), fleetEditor(*this, showFleetMenu), galaxyEditor(*this, showGalaxyMenu),
	hazardEditor(*this, showHazardMenu),
	governmentEditor(*this, showGovernmentMenu), outfitEditor(*this, showOutfitMenu), outfitterEditor(*this, showOutfitterMenu),
	planetEditor(*this, showPlanetMenu), shipEditor(*this, showShipMenu), shipyardEditor(*this, showShipyardMenu),
	systemEditor(*this, showSystemMenu)
{
	StyleColorsDarkGray();
}



Editor::~Editor()
{
	WriteAll();
}



// Saves every unsaved changes to the current plugin if any.
void Editor::SaveAll()
{
	if(!HasPlugin())
		return;

	// Commit to any unsaved changes.
	effectEditor.WriteAll();
	fleetEditor.WriteAll();
	galaxyEditor.WriteAll();
	hazardEditor.WriteAll();
	governmentEditor.WriteAll();
	outfitEditor.WriteAll();
	outfitterEditor.WriteAll();
	shipEditor.WriteAll();
	shipyardEditor.WriteAll();
	planetEditor.WriteAll();
	systemEditor.WriteAll();
}



// Writes the plugin to a file.
void Editor::WriteAll()
{
	if(!HasPlugin())
		return;

	const auto &effects = effectEditor.Changes();
	const auto &fleets = fleetEditor.Changes();
	const auto &galaxies = galaxyEditor.Changes();
	const auto &hazards = hazardEditor.Changes();
	const auto &governments = governmentEditor.Changes();
	const auto &outfits = outfitEditor.Changes();
	const auto &outfitters = outfitterEditor.Changes();
	const auto &planets = planetEditor.Changes();
	const auto &ships = shipEditor.Changes();
	const auto &shipyards = shipyardEditor.Changes();
	const auto &systems = systemEditor.Changes();

	// Save every change made to this plugin.
	for(auto &&file : pluginPaths)
	{
		DataWriter writer(file.first);

		for(auto &&pair : file.second)
		{
			const string &type = pair.first;
			const string &toSearch = pair.second;
			if(type == "planet")
			{
				auto it = find_if(planets.begin(), planets.end(),
						[&toSearch](const Planet &p) { return p.TrueName() == toSearch; });
				if(it != planets.end())
					planetEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "ship")
			{
				auto it = find_if(ships.begin(), ships.end(),
						[&toSearch](const Ship &s) { return s.TrueName() == toSearch; });
				if(it != ships.end())
					shipEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "system")
			{
				auto it = find_if(systems.begin(), systems.end(),
						[&toSearch](const System &sys) { return sys.Name() == toSearch; });
				if(it != systems.end())
					systemEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "outfit")
			{
				auto it = find_if(outfits.begin(), outfits.end(),
						[&toSearch](const Outfit &o) { return o.Name() == toSearch; });
				if(it != outfits.end())
					outfitEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "hazard")
			{
				auto it = find_if(hazards.begin(), hazards.end(),
						[&toSearch](const Hazard &h) { return h.Name() == toSearch; });
				if(it != hazards.end())
					hazardEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "government")
			{
				auto it = find_if(governments.begin(), governments.end(),
						[&toSearch](const Government &g) { return g.TrueName() == toSearch; });
				if(it != governments.end())
					governmentEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "fleet")
			{
				auto it = find_if(fleets.begin(), fleets.end(),
						[&toSearch](const Fleet &f) { return f.Name() == toSearch; });
				if(it != fleets.end())
					fleetEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "outfitter")
			{
				auto it = find_if(outfitters.begin(), outfitters.end(),
						[&toSearch](const Sale<Outfit> &o) { return o.Name() == toSearch; });
				if(it != outfitters.end())
					outfitterEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "shipyard")
			{
				auto it = find_if(shipyards.begin(), shipyards.end(),
						[&toSearch](const Sale<Ship> &s) { return s.Name() == toSearch; });
				if(it != shipyards.end())
					shipyardEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "effect")
			{
				auto it = find_if(effects.begin(), effects.end(),
						[&toSearch](const Effect &e) { return e.Name() == toSearch; });
				if(it != effects.end())
					effectEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else if(type == "galaxy")
			{
				auto it = find_if(galaxies.begin(), galaxies.end(),
						[&toSearch](const Galaxy &g) { return g.Name() == toSearch; });
				if(it != galaxies.end())
					galaxyEditor.WriteToFile(writer, &*it);
				else
					continue;
			}
			else
			{
				// If we are here then we encountered an object to save that we don't support yet.
				// In that case, we save the version in the game memory.
				auto it= unimplementedNodes.find(pair);
				assert(it != unimplementedNodes.end());
				writer.Write(it->second);
			}

			// Add an empty newline between nodes.
			writer.Write();
		}
	}
}



bool Editor::HasPlugin() const
{
	return !currentPlugin.empty();
}



bool Editor::HasUnsavedChanges() const
{
	return
		!hazardEditor.Dirty().empty()
		|| !effectEditor.Dirty().empty()
		|| !fleetEditor.Dirty().empty()
		|| !galaxyEditor.Dirty().empty()
		|| !governmentEditor.Dirty().empty()
		|| !outfitEditor.Dirty().empty()
		|| !outfitterEditor.Dirty().empty()
		|| !planetEditor.Dirty().empty()
		|| !shipEditor.Dirty().empty()
		|| !shipyardEditor.Dirty().empty()
		|| !systemEditor.Dirty().empty();
}



const std::string &Editor::GetPluginPath() const
{
	return currentPlugin;
}



PlayerInfo &Editor::Player()
{
	return player;
}



UI &Editor::GetUI()
{
	return ui;
}



UI &Editor::GetMenu()
{
	return menu;
}



void Editor::RenameObject(const std::string &type, const std::string &oldName, const std::string &newName)
{
	for(auto &&file : pluginPaths)
		for(auto &&object : file.second)
			if(object.first == type && object.second == oldName)
			{
				object.second = newName;
				break;
			}
}


void Editor::RenderMain()
{
	if(showEffectMenu)
		effectEditor.Render();
	if(showFleetMenu)
		fleetEditor.Render();
	if(showGalaxyMenu)
		galaxyEditor.Render();
	if(showHazardMenu)
		hazardEditor.Render();
	if(showGovernmentMenu)
		governmentEditor.Render();
	if(showOutfitMenu)
		outfitEditor.Render();
	if(showOutfitterMenu)
		outfitterEditor.Render();
	if(showShipMenu)
		shipEditor.Render();
	if(showShipyardMenu)
		shipyardEditor.Render();
	if(showSystemMenu)
		systemEditor.Render();
	else
		systemEditor.AlwaysRender();
	if(showPlanetMenu)
		planetEditor.Render();

	bool newPluginDialog = false;
	bool openPluginDialog = false;
	if(ImGui::BeginMainMenuBar())
	{
		if(ImGui::BeginMenu("File"))
		{
			ImGui::MenuItem("New Plugin", nullptr, &newPluginDialog);
			ImGui::MenuItem("Open Plugin", nullptr, &openPluginDialog);
			if(!currentPlugin.empty())
				ImGui::MenuItem(("\"" + currentPluginName + "\" loaded").c_str(), nullptr, false, false);
			if(ImGui::MenuItem("Save All", nullptr, false, HasPlugin()))
			{
				SaveAll();
				WriteAll();
			}
			if(ImGui::MenuItem("Quit"))
				ShowConfirmationDialog();
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Editors"))
		{
			ImGui::MenuItem("Effect Editor", nullptr, &showEffectMenu);
			ImGui::MenuItem("Fleet Editor", nullptr, &showFleetMenu);
			ImGui::MenuItem("Galaxy Editor", nullptr, &showGalaxyMenu);
			ImGui::MenuItem("Hazard Editor", nullptr, &showHazardMenu);
			ImGui::MenuItem("Government Editor", nullptr, &showGovernmentMenu);
			ImGui::MenuItem("Outfit Editor", nullptr, &showOutfitMenu);
			ImGui::MenuItem("Outfitter Editor", nullptr, &showOutfitterMenu);
			ImGui::MenuItem("Ship Editor", nullptr, &showShipMenu);
			ImGui::MenuItem("Shipyard Editor", nullptr, &showShipyardMenu);
			if(ImGui::MenuItem("System Editor", nullptr, &showSystemMenu))
			{
				auto *panel = dynamic_cast<MapEditorPanel *>(menu.Top().get());
				if(!panel && !dynamic_cast<MainEditorPanel *>(menu.Top().get()))
					menu.Push(new MapEditorPanel(player, &planetEditor, &systemEditor));
			}
			ImGui::MenuItem("Planet Editor", nullptr, &showPlanetMenu);
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Open Map Editor"))
				if(!dynamic_cast<MapEditorPanel *>(menu.Top().get()))
					menu.Push(new MapEditorPanel(player, &planetEditor, &systemEditor));
			if(ImGui::MenuItem("Open In-System Editor"))
				if(!dynamic_cast<MainEditorPanel *>(menu.Top().get()))
					menu.Push(new MainEditorPanel(player, &planetEditor, &systemEditor));
			if(ImGui::MenuItem("Reload Plugin Resources", nullptr, false, HasPlugin()))
				ReloadPluginResources();
			ImGui::EndMenu();
		}

		const auto &dirtyEffects = effectEditor.Dirty();
		const auto &dirtyFleets = fleetEditor.Dirty();
		const auto &dirtyGalaxies = galaxyEditor.Dirty();
		const auto &dirtyHazards = hazardEditor.Dirty();
		const auto &dirtyGovernments = governmentEditor.Dirty();
		const auto &dirtyOutfits = outfitEditor.Dirty();
		const auto &dirtyOutfitters = outfitterEditor.Dirty();
		const auto &dirtyPlanets = planetEditor.Dirty();
		const auto &dirtyShips = shipEditor.Dirty();
		const auto &dirtyShipyards = shipyardEditor.Dirty();
		const auto &dirtySystems = systemEditor.Dirty();
		const bool hasChanges = HasUnsavedChanges();

		if(hasChanges)
			ImGui::PushStyleColor(ImGuiCol_PopupBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		if(ImGui::BeginMenu("Unsaved Changes", hasChanges))
		{
			if(!dirtyEffects.empty() && ImGui::BeginMenu("Effects"))
			{
				for(auto &&e : dirtyEffects)
					ImGui::MenuItem(e.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyFleets.empty() && ImGui::BeginMenu("Fleets"))
			{
				for(auto &&f : dirtyFleets)
					ImGui::MenuItem(f.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyGalaxies.empty() && ImGui::BeginMenu("Galaxies"))
			{
				for(auto &&g : dirtyGalaxies)
					ImGui::MenuItem(g.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyHazards.empty() && ImGui::BeginMenu("Hazards"))
			{
				for(auto &&h : dirtyHazards)
					ImGui::MenuItem(h.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyGovernments.empty() && ImGui::BeginMenu("Governments"))
			{
				for(auto &&g : dirtyGovernments)
					ImGui::MenuItem(g.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyOutfits.empty() && ImGui::BeginMenu("Outfits"))
			{
				for(auto &&o : dirtyOutfits)
					ImGui::MenuItem(o.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyOutfitters.empty() && ImGui::BeginMenu("Outfitters"))
			{
				for(auto &&o : dirtyOutfitters)
					ImGui::MenuItem(o.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyPlanets.empty() && ImGui::BeginMenu("Planets"))
			{
				for(auto &&p : dirtyPlanets)
					ImGui::MenuItem(p.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyShips.empty() && ImGui::BeginMenu("Ships"))
			{
				for(auto &&s : dirtyShips)
					ImGui::MenuItem(s.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtyShipyards.empty() && ImGui::BeginMenu("Shipyards"))
			{
				for(auto &&s : dirtyShipyards)
					ImGui::MenuItem(s.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			if(!dirtySystems.empty() && ImGui::BeginMenu("Systems"))
			{
				for(auto &&sys : dirtySystems)
					ImGui::MenuItem(sys.second.c_str(), nullptr, false, false);
				ImGui::EndMenu();
			}

			ImGui::EndMenu();
		}
		if(hasChanges)
			ImGui::PopStyleColor();

		if(ImGui::BeginMenu("Themes"))
		{
			if(ImGui::MenuItem("Dark Theme"))
				ImGui::StyleColorsDark();
			if(ImGui::MenuItem("Light Theme"))
				ImGui::StyleColorsLight();
			if(ImGui::MenuItem("Classic Theme"))
				ImGui::StyleColorsClassic();
			if(ImGui::MenuItem("Yellow Theme"))
				StyleColorsYellow();
			if(ImGui::MenuItem("Dark Gray Theme"))
				StyleColorsDarkGray();
			ImGui::EndMenu();
		}

		ImGui::EndMainMenuBar();
	}

	if(newPluginDialog)
		ImGui::OpenPopup("New Plugin");
	if(openPluginDialog)
		ImGui::OpenPopup("Open Plugin");
	if(showConfirmationDialog)
		ImGui::OpenPopup("Confirmation Dialog");

	if(ImGui::BeginPopupModal("New Plugin", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static string newPlugin;
		ImGui::Text("Create new plugin:");
		bool create = ImGui::InputText("", &newPlugin, ImGuiInputTextFlags_EnterReturnsTrue);
		if(ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
			newPlugin.clear();
		}
		ImGui::SameLine();
		if(newPlugin.empty())
			ImGui::PushDisabled();
		bool dontDisable = false;
		if(ImGui::Button("Ok") || create)
		{
			NewPlugin(newPlugin);
			ImGui::CloseCurrentPopup();
			newPlugin.clear();
			dontDisable = true;
		}
		if(newPlugin.empty() && !dontDisable)
			ImGui::PopDisabled();
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopupModal("Open Plugin", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		static string openPlugin;
		ImGui::Text("Open plugin:");
		if(ImGui::BeginCombo("", openPlugin.c_str()))
		{
			auto plugins = Files::ListDirectories(Files::Config() + "plugins/");
			for(auto plugin : plugins)
			{
				plugin.pop_back();
				plugin = Files::Name(plugin);
				if(ImGui::Selectable(plugin.c_str()))
					openPlugin = plugin;
			}

			ImGui::EndCombo();
		}
		if(ImGui::Button("Cancel"))
		{
			ImGui::CloseCurrentPopup();
			openPlugin.clear();
		}
		ImGui::SameLine();
		if(openPlugin.empty())
			ImGui::PushDisabled();
		bool dontDisable = false;
		if(ImGui::Button("Ok") || (!openPlugin.empty() && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))))
		{
			OpenPlugin(openPlugin);
			ImGui::CloseCurrentPopup();
			openPlugin.clear();
			dontDisable = true;
		}
		if(openPlugin.empty() && !dontDisable)
			ImGui::PopDisabled();
		ImGui::EndPopup();
	}
	if(ImGui::BeginPopupModal("Confirmation Dialog", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::Text("You have unsaved changes. Are you sure you want to quit?");
		if(ImGui::Button("No"))
		{
			showConfirmationDialog = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(ImGui::Button("Yes"))
		{
			menu.Quit();
			showConfirmationDialog = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if(!HasPlugin())
			ImGui::PushDisabled();
		if(ImGui::Button("Save All and Quit"))
		{
			menu.Quit();
			showConfirmationDialog = false;
			SaveAll();
			ImGui::CloseCurrentPopup();
		}
		if(!HasPlugin())
		{
			ImGui::PopDisabled();
			if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip("You don't have a plugin loaded to save the changes to.");
		}
		ImGui::EndPopup();
	}
}



void Editor::ShowConfirmationDialog()
{
	if(HasUnsavedChanges())
		showConfirmationDialog = true;
	else
		// No unsaved changes, so just quit.
		menu.Quit();
}



void Editor::ReloadPluginResources()
{
	if(!HasPlugin())
		return;

	string directoryPath = currentPlugin + "images/";
	size_t start = directoryPath.size();

	vector<string> imageFiles = Files::RecursiveList(directoryPath);
	map<string, shared_ptr<ImageSet>> images;
	for(const string &path : imageFiles)
		if(ImageSet::IsImage(path))
		{
			string name = ImageSet::Name(path.substr(start));

			shared_ptr<ImageSet> &imageSet = images[name];
			if(!imageSet)
				imageSet.reset(new ImageSet(name));
			imageSet->Add(path);
		}

	for(const auto &it : images)
	{
		// This should never happen, but just in case:
		if(!it.second)
			continue;

		// Check that the image set is complete.
		it.second->ValidateFrames();
		// For landscapes, remember all the source files but don't load them yet.
		GameData::spriteQueue.Add(it.second);
	}
	Music::Init({currentPlugin});
	GameData::spriteQueue.Finish();
}



void AddNode(Editor &editor, const std::string &file, const std::string &key, const std::string &name)
{
	editor.pluginPaths[editor.currentPlugin + "data/" + file].emplace_back(std::make_pair(key, name));
}



void Editor::NewPlugin(const string &plugin)
{
	// Don't create a new plugin it if already exists.
	auto pluginsPath = Files::Config() + "plugins/";
	auto plugins = Files::ListDirectories(pluginsPath);
	for(const auto &existing : plugins)
		if(existing == plugin)
			return OpenPlugin(plugin);

	Files::CreateNewDirectory(pluginsPath + plugin);
	Files::CreateNewDirectory(pluginsPath + plugin + "/data");
	OpenPlugin(plugin);
}



void Editor::OpenPlugin(const string &plugin)
{
	const string path = Files::Config() + "plugins/" + plugin + "/";
	if(!Files::Exists(path))
		return;

	pluginPaths.clear();
	unimplementedNodes.clear();

	// Special case: If we didn't load a plugin yet don't discard changes.
	if(!currentPlugin.empty())
	{
		effectEditor.Clear();
		fleetEditor.Clear();
		galaxyEditor.Clear();
		hazardEditor.Clear();
		governmentEditor.Clear();
		outfitEditor.Clear();
		outfitterEditor.Clear();
		shipEditor.Clear();
		shipyardEditor.Clear();
		systemEditor.Clear();
		planetEditor.Clear();
	}

	currentPlugin = path;
	currentPluginName = plugin;

	GameData::baseEffects.clear();
	GameData::baseFleets.clear();
	GameData::baseGalaxies.clear();
	GameData::baseHazards.clear();
	GameData::baseGovernments.clear();
	GameData::baseOutfits.clear();
	GameData::baseOutfitSales.clear();
	GameData::baseShips.clear();
	GameData::baseShipSales.clear();
	GameData::baseSystems.clear();
	GameData::basePlanets.clear();
	GameData::LoadData(&currentPlugin);
	player.PartialLoad();

	// We need to save everything the specified plugin loads.
	auto files = Files::RecursiveList(path + "data/");
	for(const auto &file : files)
	{
		DataFile data(file);
		for(const auto &node : data)
		{
			const string &key = node.Token(0);
			if(node.Size() < 2)
				continue;
			const string &value = node.Token(1);

			if(key == "planet")
				planetEditor.WriteToPlugin(GameData::Planets().Get(value), false);
			else if(key == "ship")
			{
				// We might have a variant instead of a normal ship definition.
				if(node.Size() >= 3)
				{
					shipEditor.WriteToPlugin(GameData::Ships().Get(node.Token(2)), false);
					pluginPaths[file].emplace_back(key, node.Token(2));
					continue;
				}
				else
					shipEditor.WriteToPlugin(GameData::Ships().Get(value), false);
			}
			else if(key == "system")
				systemEditor.WriteToPlugin(GameData::Systems().Get(value), false);
			else if(key == "outfit")
				outfitEditor.WriteToPlugin(GameData::Outfits().Get(value), false);
			else if(key == "hazard")
				hazardEditor.WriteToPlugin(GameData::Hazards().Get(value), false);
			else if(key == "government")
				governmentEditor.WriteToPlugin(GameData::Governments().Get(value), false);
			else if(key == "fleet")
				fleetEditor.WriteToPlugin(GameData::Fleets().Get(value), false);
			else if(key == "outfitter")
				outfitterEditor.WriteToPlugin(GameData::Outfitters().Get(value), false);
			else if(key == "shipyard")
				shipyardEditor.WriteToPlugin(GameData::Shipyards().Get(value), false);
			else if(key == "effect")
				effectEditor.WriteToPlugin(GameData::Effects().Get(value), false);
			else if(key == "galaxy")
				galaxyEditor.WriteToPlugin(GameData::Galaxies().Get(value), false);
			else
				unimplementedNodes.emplace(std::make_pair(key, value), node);

			bool alreadyExists = false;
			for(const auto &file : pluginPaths)
			{
				for(const auto &pair : file.second)
					if(key != "phrase" && pair.first == key && pair.second == value)
					{
						alreadyExists = true;
						break;
					}
				if(alreadyExists)
					break;
			}
			if(alreadyExists)
				node.PrintTrace("Duplicate node found. This is only partially supported by the game (and by this editor) so it is recommended to avoid duplicating nodes.");
			else
				pluginPaths[file].emplace_back(key, value);
		}
	}
}



void Editor::StyleColorsYellow()
{
	// Copyright: CookiePLMonster
	// https://github.com/ocornut/imgui/issues/707#issuecomment-622934113
	ImGuiStyle *style = &ImGui::GetStyle();
	ImVec4 *colors = style->Colors;

	colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.44f, 0.44f, 0.44f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.11f, 0.11f, 0.11f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.06f, 0.06f, 0.06f, 0.53f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.47f, 0.47f, 0.47f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.81f, 0.83f, 0.81f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.93f, 0.65f, 0.14f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.21f, 0.21f, 0.21f, 1.00f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
	colors[ImGuiCol_Tab] = ImVec4(0.51f, 0.36f, 0.15f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.91f, 0.64f, 0.13f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.78f, 0.55f, 0.21f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.07f, 0.10f, 0.15f, 0.97f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.14f, 0.26f, 0.42f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}



void Editor::StyleColorsDarkGray()
{
	// Copyright: Raikiri
	// https://github.com/ocornut/imgui/issues/707#issuecomment-512669512
	ImGui::GetStyle().FrameRounding = 4.0f;
	ImGui::GetStyle().GrabRounding = 4.0f;

	ImVec4 *colors = ImGui::GetStyle().Colors;
	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
	colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
	colors[ImGuiCol_Border] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
	colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.12f, 0.20f, 0.28f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.09f, 0.12f, 0.14f, 1.00f);
	colors[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.12f, 0.14f, 0.65f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
	colors[ImGuiCol_MenuBarBg] = ImVec4(0.15f, 0.18f, 0.22f, 1.00f);
	colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.39f);
	colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.18f, 0.22f, 0.25f, 1.00f);
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.09f, 0.21f, 0.31f, 1.00f);
	colors[ImGuiCol_CheckMark] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrab] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
	colors[ImGuiCol_SliderGrabActive] = ImVec4(0.37f, 0.61f, 1.00f, 1.00f);
	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.56f, 1.00f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.06f, 0.53f, 0.98f, 1.00f);
	colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.29f, 0.55f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_SeparatorHovered] = ImVec4(0.10f, 0.40f, 0.75f, 0.78f);
	colors[ImGuiCol_SeparatorActive] = ImVec4(0.10f, 0.40f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGrip] = ImVec4(0.26f, 0.59f, 0.98f, 0.25f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.67f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.59f, 0.98f, 0.95f);
	colors[ImGuiCol_Tab] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.59f, 0.98f, 0.80f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.25f, 0.29f, 1.00f);
	colors[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.11f, 0.15f, 0.17f, 1.00f);
	colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
	colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
	colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
	colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
	colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
	colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
	colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}
