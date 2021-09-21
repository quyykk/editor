/* OutfitEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "OutfitEditor.h"

#include "Audio.h"
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
#include "Sound.h"
#include "SpriteSet.h"
#include "Sprite.h"
#include "System.h"
#include "UI.h"
#include "Weapon.h"

#include <cassert>
#include <map>

using namespace std;



OutfitEditor::OutfitEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Outfit>(editor, show)
{
}



void OutfitEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Outfit Editor", &show, ImGuiWindowFlags_MenuBar))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	bool showNewOutfit = false;
	bool showRenameOutfit = false;
	bool showCloneOutfit = false;
	if(ImGui::BeginMenuBar())
	{
		if(ImGui::BeginMenu("Outfit"))
		{
			const bool alreadyDefined = object && !GameData::baseOutfits.Has(object->name);
			ImGui::MenuItem("New", nullptr, &showNewOutfit);
			ImGui::MenuItem("Rename", nullptr, &showRenameOutfit, alreadyDefined);
			ImGui::MenuItem("Clone", nullptr, &showCloneOutfit, object);
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
				if(!found && GameData::baseOutfits.Has(object->name))
					*object = *GameData::baseOutfits.Get(object->name);
				else if(!found)
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
					GameData::Outfits().Erase(object->name);
					object = nullptr;
				}
			}
			if(ImGui::MenuItem("Delete", nullptr, false, alreadyDefined))
			{
				if(find_if(Changes().begin(), Changes().end(), [this](const Outfit &outfit)
							{
								return outfit.name == object->name;
							}) != Changes().end())
				{
					SetDirty("[deleted]");
					DeleteFromChanges();
				}
				else
					SetClean();
				GameData::Outfits().Erase(object->name);
				object = nullptr;
			}
			ImGui::EndMenu();
		}
		if(ImGui::BeginMenu("Tools"))
		{
			if(ImGui::MenuItem("Add to Cargo", nullptr, false, object && editor.Player().Cargo().Free() >= -object->Attributes().Get("outfit space")))
				editor.Player().Cargo().Add(object);
			if(!object || editor.Player().Cargo().Free() < -object->Attributes().Get("outfit space"))
				if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					if(!object)
						ImGui::SetTooltip("Select an outfit first.");
					else
						ImGui::SetTooltip("Not enough space in your cargo hold for this outfit.");
				}
			if(ImGui::MenuItem("Add to Flagship", nullptr, false, object && editor.Player().Flagship()->Attributes().CanAdd(*object, 1) > 0))
				editor.Player().Flagship()->AddOutfit(object, 1);
			if(!object || editor.Player().Flagship()->Attributes().CanAdd(*object, 1) <= 0)
				if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				{
					if(!object)
						ImGui::SetTooltip("Select an outfit first.");
					else
						ImGui::SetTooltip("Not enough outfit space in your flagship.");
				}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}

	if(showNewOutfit)
		ImGui::OpenPopup("New Outfit");
	if(showRenameOutfit)
		ImGui::OpenPopup("Rename Outfit");
	if(showCloneOutfit)
		ImGui::OpenPopup("Clone Outfit");
	ImGui::BeginSimpleNewModal("New Outfit", [this](const string &name)
			{
				auto *newOutfit = const_cast<Outfit *>(GameData::Outfits().Get(name));
				newOutfit->name = name;
				newOutfit->isDefined = true;
				object = newOutfit;
				SetDirty();
			});
	ImGui::BeginSimpleRenameModal("Rename Outfit", [this](const string &name)
			{
				DeleteFromChanges();
				editor.RenameObject(keyFor<Outfit>(), object->name, name);
				GameData::Outfits().Rename(object->name, name);
				object->name = name;
				WriteToPlugin(object, false);
				SetDirty();
			});
	ImGui::BeginSimpleCloneModal("Clone Outfit", [this](const string &name)
			{
				auto *clone = const_cast<Outfit *>(GameData::Outfits().Get(name));
				*clone = *object;
				object = clone;

				object->name = name;
				SetDirty();
			});
	if(ImGui::InputCombo("outfit", &searchBox, &object, GameData::Outfits()))
		searchBox.clear();

	ImGui::Separator();
	ImGui::Spacing();
	if(object)
		RenderOutfit();
	ImGui::End();
}



void OutfitEditor::RenderOutfit()
{
	static string str;
	ImGui::Text("outfit: %s", object->Name().c_str());
	if(ImGui::BeginCombo("category", object->Category().c_str()))
	{
		int index = 0;
		for(const auto &category : GameData::Category(CategoryType::OUTFIT))
		{
			const bool selected = object->Category().c_str() == category;
			if(ImGui::Selectable(category.c_str(), selected))
			{
				object->category = category;
				SetDirty();
			}
			++index;

			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	if(ImGui::InputText("plural", &object->pluralName))
		SetDirty();
	if(ImGui::InputInt64Ex("cost", &object->cost))
		SetDirty();
	if(ImGui::InputDoubleEx("mass", &object->mass))
		SetDirty();

	str.clear();
	if(object->flotsamSprite)
		str = object->flotsamSprite->Name();
	static Sprite *flotsamSprite = nullptr;
	if(ImGui::InputCombo("flotsam sprite", &str, &flotsamSprite, SpriteSet::GetSprites()))
	{
		object->flotsamSprite = flotsamSprite;
		SetDirty();
	}

	str.clear();
	if(object->thumbnail)
		str = object->thumbnail->Name();
	static Sprite *thumbnailSprite = nullptr;
	if(ImGui::InputCombo("thumbnail", &str, &thumbnailSprite, SpriteSet::GetSprites()))
	{
		object->thumbnail = thumbnailSprite;
		SetDirty();
	}

	if(ImGui::InputTextMultiline("description", &object->description, ImVec2(), ImGuiInputTextFlags_EnterReturnsTrue))
		SetDirty();

	int index = 0;
	if(ImGui::TreeNode("licenses"))
	{
		auto toRemove = object->licenses.end();
		for(auto it = object->licenses.begin(); it != object->licenses.end(); ++it)
		{
			ImGui::PushID(index++);
			if(ImGui::InputText("", &*it, ImGuiInputTextFlags_EnterReturnsTrue))
			{
				if(it->empty())
					toRemove = it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(toRemove != object->licenses.end())
			object->licenses.erase(toRemove);
		ImGui::Spacing();

		static string addLicense;
		if(ImGui::InputText("##license", &addLicense, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->licenses.push_back(move(addLicense));
			SetDirty();
		}
		ImGui::TreePop();

	}

	if(ImGui::TreeNode("attributes"))
	{
		for(auto &it : object->attributes)
			if(it.second)
				if(ImGui::InputDoubleEx(it.first, &it.second, ImGuiInputTextFlags_EnterReturnsTrue))
					SetDirty();

		ImGui::Spacing();
		static string addAttribute;
		ImGui::InputText("add attribute name", &addAttribute);
		static double value = 0.;
		if(ImGui::InputDoubleEx("add attribute value", &value, ImGuiInputTextFlags_EnterReturnsTrue))
		{
			object->Set(addAttribute.c_str(), value);
			object->Set(addAttribute.c_str(), value);
			addAttribute.clear();
			value = 0.;
			SetDirty();
		}

		ImGui::TreePop();
	}

	bool openSprites = ImGui::TreeNode("sprites");
	if(ImGui::BeginPopupContextItem())
	{
		if(ImGui::Selectable("Add flare sprite"))
		{
			object->flareSprites.emplace_back(Body(), 1);
			SetDirty();
		}
		if(ImGui::Selectable("Add reverse flare sprite"))
		{
			object->reverseFlareSprites.emplace_back(Body(), 1);
			SetDirty();
		}
		if(ImGui::Selectable("Add steering flare sprite"))
		{
			object->steeringFlareSprites.emplace_back(Body(), 1);
			SetDirty();
		}
		ImGui::EndPopup();
	}
	if(openSprites)
	{
		RenderSprites("flare sprite", object->flareSprites);
		RenderSprites("reverse flare sprite", object->reverseFlareSprites);
		RenderSprites("steering flare sprite", object->steeringFlareSprites);
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("sounds"))
	{
		RenderSound("flare sound", object->flareSounds);
		RenderSound("reverse flare sound", object->reverseFlareSounds);
		RenderSound("steering flare sound", object->steeringFlareSounds);
		RenderSound("hyperdrive sound", object->hyperSounds);
		RenderSound("hyperdrive in sound", object->hyperInSounds);
		RenderSound("hyperdrive out sound", object->hyperOutSounds);
		RenderSound("jump sound", object->jumpSounds);
		RenderSound("jump in sound", object->jumpInSounds);
		RenderSound("jump out sound", object->jumpOutSounds);
		ImGui::TreePop();
	}
	if(ImGui::TreeNode("effects"))
	{
		RenderEffect("afterburner effect", object->afterburnerEffects);
		RenderEffect("jump effect", object->jumpEffects);
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("weapon"))
	{
		bool isClustered = false;
		if(ImGui::Checkbox("stream", &object->isStreamed))
			SetDirty();
		if(ImGui::Checkbox("cluster", &isClustered))
			SetDirty();
		if(ImGui::Checkbox("safe", &object->isSafe))
			SetDirty();
		if(ImGui::Checkbox("phasing", &object->isPhasing))
			SetDirty();
		if(ImGui::Checkbox("no damage scaling", &object->isDamageScaled))
			SetDirty();
		if(ImGui::Checkbox("parallel", &object->isParallel))
			SetDirty();
		if(ImGui::Checkbox("gravitational", &object->isGravitational))
			SetDirty();
		RenderElement(&object->sprite, "sprite");
		RenderElement(&object->hardpointSprite, "hardpoint sprite");
		static string value;
		if(object->sound)
			value = object->sound->Name();
		if(ImGui::InputText("sound", &value, ImGuiInputTextFlags_EnterReturnsTrue))
			if(Audio::Has(value))
			{
				object->sound = Audio::Get(value);
				SetDirty();
			}
		if(ImGui::TreeNode("ammo"))
		{
			value.clear();
			if(object->ammo.first)
				value = object->ammo.first->Name();
			if(ImGui::InputText("##ammo", &value, ImGuiInputTextFlags_EnterReturnsTrue))
				if(GameData::Outfits().Has(value))
				{
					object->ammo.first = GameData::Outfits().Get(value);
					SetDirty();
				}
			ImGui::SameLine();
			if(ImGui::InputInt("##usage", &object->ammo.second))
				SetDirty();
			ImGui::TreePop();
		}
		value.clear();
		if(object->icon)
			value = object->icon->Name();
		static Sprite *iconSprite = nullptr;
		if(ImGui::InputCombo("icon", &value, &iconSprite, SpriteSet::GetSprites()))
		{
			object->icon = iconSprite;
			SetDirty();
		}
		RenderEffect("fire effect", object->fireEffects);
		RenderEffect("live effect", object->liveEffects);
		RenderEffect("hit effect", object->hitEffects);
		RenderEffect("target effect", object->targetEffects);
		RenderEffect("die effect", object->dieEffects);

		bool submunitionOpen = ImGui::TreeNode("submunitions");
		if(ImGui::BeginPopupContextItem())
		{
			if(ImGui::Selectable("Add submunition"))
			{
				object->submunitions.emplace_back(nullptr, 1);
				SetDirty();
			}
			ImGui::EndPopup();
		}
		if(submunitionOpen)
		{
			index = 0;
			auto toRemove = object->submunitions.end();
			for(auto it = object->submunitions.begin(); it != object->submunitions.end(); ++it)
			{
				ImGui::PushID(index++);
				static string outfitName;
				outfitName.clear();
				if(it->weapon)
					outfitName = it->weapon->name;
				bool open = ImGui::TreeNode("submunition", "submunition: %s %zu", outfitName.c_str(), it->count);
				if(ImGui::BeginPopupContextItem())
				{
					if(ImGui::Selectable("Remove"))
						toRemove = it;
					ImGui::EndPopup();
				}
				if(open)
				{
					if(ImGui::InputText("outfit", &outfitName, ImGuiInputTextFlags_EnterReturnsTrue))
						if(GameData::Outfits().Has(outfitName))
						{
							it->weapon = GameData::Outfits().Get(outfitName);
							SetDirty();
						}
					if(ImGui::InputSizeTEx("count", &it->count))
						SetDirty();
					double value = it->facing.Degrees();
					if(ImGui::InputDoubleEx("facing", &value))
					{
						it->facing = Angle(value);
						SetDirty();
					}
					double offset[2] = {it->offset.X(), it->offset.Y()};
					if(ImGui::InputDouble2Ex("offset", offset))
					{
						it->offset.Set(offset[0], offset[1]);
						SetDirty();
					}
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			if(toRemove != object->submunitions.end())
			{
				object->submunitions.erase(toRemove);
				SetDirty();
			}
			ImGui::TreePop();
		}

		if(ImGui::InputInt("lifetime", &object->lifetime))
		{
			object->lifetime = max(0, object->lifetime);
			SetDirty();
		}
		if(ImGui::InputInt("random lifetime", &object->randomLifetime))
		{
			object->randomLifetime = max(0, object->randomLifetime);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("reload", &object->reload))
		{
			object->reload = max(1., object->reload);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("burst reload", &object->burstReload))
		{
			object->burstReload = max(1., object->burstReload);
			SetDirty();
		}
		if(ImGui::InputInt("burst count", &object->burstCount))
		{
			object->burstCount = max(1, object->burstCount);
			SetDirty();
		}
		if(ImGui::InputInt("homing", &object->homing))
			SetDirty();
		if(ImGui::InputInt("missile strength", &object->missileStrength))
		{
			object->missileStrength = max(0, object->missileStrength);
			SetDirty();
		}
		if(ImGui::InputInt("anti-missile", &object->antiMissile))
		{
			object->antiMissile = max(0, object->antiMissile);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("velocity", &object->velocity))
			SetDirty();
		if(ImGui::InputDoubleEx("random velocity", &object->randomVelocity))
			SetDirty();
		if(ImGui::InputDoubleEx("acceleration", &object->acceleration))
			SetDirty();
		if(ImGui::InputDoubleEx("drag", &object->drag))
			SetDirty();
		double hardpointOffset[2] = {object->hardpointOffset.X(), -object->hardpointOffset.Y()};
		if(ImGui::InputDouble2Ex("hardpoint offset", hardpointOffset))
		{
			object->hardpointOffset.Set(hardpointOffset[0], -hardpointOffset[1]);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("turn", &object->turn))
			SetDirty();
		if(ImGui::InputDoubleEx("inaccuracy", &object->inaccuracy))
			SetDirty();
		if(ImGui::InputDoubleEx("turret turn", &object->turretTurn))
			SetDirty();
		if(ImGui::InputDoubleEx("tracking", &object->tracking))
		{
			object->tracking = max(0., min(1., object->tracking));
			SetDirty();
		}
		if(ImGui::InputDoubleEx("optical tracking", &object->opticalTracking))
		{
			object->opticalTracking = max(0., min(1., object->opticalTracking));
			SetDirty();
		}
		if(ImGui::InputDoubleEx("infrared tracking", &object->infraredTracking))
		{
			object->infraredTracking = max(0., min(1., object->infraredTracking));
			SetDirty();
		}
		if(ImGui::InputDoubleEx("radar tracking", &object->radarTracking))
		{
			object->radarTracking = max(0., min(1., object->radarTracking));
			SetDirty();
		}
		if(ImGui::InputDoubleEx("firing energy", &object->firingEnergy))
			SetDirty();
		if(ImGui::InputDoubleEx("firing force", &object->firingForce))
			SetDirty();
		if(ImGui::InputDoubleEx("firing fuel", &object->firingFuel))
			SetDirty();
		if(ImGui::InputDoubleEx("firing heat", &object->firingHeat))
			SetDirty();
		if(ImGui::InputDoubleEx("firing hull", &object->firingHull))
			SetDirty();
		if(ImGui::InputDoubleEx("firing shields", &object->firingShields))
			SetDirty();
		if(ImGui::InputDoubleEx("firing ion", &object->firingIon))
			SetDirty();
		if(ImGui::InputDoubleEx("firing slowing", &object->firingSlowing))
			SetDirty();
		if(ImGui::InputDoubleEx("firing disruption", &object->firingDisruption))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing energy", &object->relativeFiringEnergy))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing heat", &object->relativeFiringHeat))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing fuel", &object->relativeFiringFuel))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing hull", &object->relativeFiringHull))
			SetDirty();
		if(ImGui::InputDoubleEx("relative firing shields", &object->relativeFiringShields))
			SetDirty();
		if(ImGui::InputDoubleEx("split range", &object->splitRange))
		{
			object->splitRange = max(0., object->splitRange);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("trigger radius", &object->triggerRadius))
		{
			object->triggerRadius = max(0., object->triggerRadius);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("blast radius", &object->blastRadius))
		{
			object->blastRadius = max(0., object->blastRadius);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("shield damage", &object->damage[Weapon::SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hull damage", &object->damage[Weapon::HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("fuel damage", &object->damage[Weapon::FUEL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("heat damage", &object->damage[Weapon::HEAT_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("energy damage", &object->damage[Weapon::ENERGY_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("ion damage", &object->damage[Weapon::ION_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("disruption damage", &object->damage[Weapon::DISRUPTION_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("slowing damage", &object->damage[Weapon::SLOWING_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relatile shield damage", &object->damage[Weapon::RELATIVE_SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative hull damage", &object->damage[Weapon::RELATIVE_HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative fuel damage", &object->damage[Weapon::RELATIVE_FUEL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative heat damage", &object->damage[Weapon::RELATIVE_HEAT_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative energy damage", &object->damage[Weapon::RELATIVE_ENERGY_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("hit force", &object->damage[Weapon::HIT_FORCE]))
			SetDirty();
		if(ImGui::InputDoubleEx("piecing", &object->piercing))
		{
			object->piercing = max(0., object->piercing);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("range override", &object->rangeOverride))
		{
			object->rangeOverride = max(0., object->rangeOverride);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("velocity override", &object->velocityOverride))
		{
			object->velocityOverride = max(0., object->velocityOverride);
			SetDirty();
		}
		double dropoff[2] = {object->damageDropoffRange.first, object->damageDropoffRange.second};
		if(ImGui::InputDouble2Ex("damage dropoff", dropoff))
		{
			object->damageDropoffRange.first = max(0., dropoff[0]);
			object->damageDropoffRange.second = dropoff[1];
			object->hasDamageDropoff = true;
			SetDirty();
		}
		if(ImGui::InputDoubleEx("dropoff modifier", &object->damageDropoffModifier))
		{
			object->damageDropoffModifier = max(0., object->damageDropoffModifier);
			SetDirty();
		}

		if(object->burstReload > object->reload)
			object->burstReload = object->reload;
		if(object->damageDropoffRange.first > object->damageDropoffRange.second)
			object->damageDropoffRange.second = object->Range();
		object->isStreamed |= !(object->MissileStrength() || object->AntiMissile());
		object->isStreamed &= !isClustered;
		ImGui::TreePop();
	}
}



void OutfitEditor::WriteToFile(DataWriter &writer, const Outfit *outfit)
{
	const auto *diff = GameData::baseOutfits.Has(outfit->name)
		? GameData::baseOutfits.Get(outfit->name)
		: nullptr;

	writer.Write("outfit", outfit->Name());
	writer.BeginChild();

	if(!diff || outfit->category != diff->category)
		if(!outfit->category.empty() || diff)
			writer.Write("category", outfit->Category().c_str());
	if(!diff || outfit->pluralName != diff->category)
		if(outfit->pluralName != outfit->name + "s" || diff)
			writer.Write("plural", outfit->pluralName);
	if(!diff || outfit->cost != diff->cost)
		if(outfit->cost || diff)
			writer.Write("cost", outfit->cost);
	if(!diff || outfit->mass != diff->mass)
		if(outfit->mass || diff)
			writer.Write("mass", outfit->mass);
	if(!diff || outfit->flotsamSprite != diff->flotsamSprite)
		if(outfit->flotsamSprite || diff)
			writer.Write("flotsam sprite", outfit->flotsamSprite->Name());

	if(!diff || outfit->thumbnail != diff->thumbnail)
		if(outfit->thumbnail || diff)
			writer.Write("thumbnail", outfit->thumbnail->Name());
	if(!diff || outfit->licenses != diff->licenses) 
		if(!outfit->licenses.empty())
		{
			writer.WriteToken("licenses");
			for(auto &&license : outfit->licenses)
				writer.WriteToken(license);
			writer.Write();
		}

	if(!diff || outfit->attributes.AsBase() != diff->attributes.AsBase())
		for(auto it = outfit->attributes.begin(); it != outfit->attributes.end(); ++it)
			if(!diff || !Count(diff->attributes.AsBase(), *it))
				writer.Write(it->first, it->second);

	if(!diff || outfit->flareSprites != diff->flareSprites)
		for(auto &&flareSprite : outfit->flareSprites)
			for(int i = 0; i < flareSprite.second; ++i)
				flareSprite.first.SaveSprite(writer, "flare sprite");
	if(!diff || outfit->reverseFlareSprites != diff->reverseFlareSprites)
		for(auto &&reverseFlareSprite : outfit->reverseFlareSprites)
			for(int i = 0; i < reverseFlareSprite.second; ++i)
				reverseFlareSprite.first.SaveSprite(writer, "reverse flare sprite");
	if(!diff || outfit->steeringFlareSprites != diff->steeringFlareSprites)
		for(auto &&steeringFlareSprite : outfit->steeringFlareSprites)
			for(int i = 0; i < steeringFlareSprite.second; ++i)
				steeringFlareSprite.first.SaveSprite(writer, "steering flare sprite");
	if(!diff || outfit->flareSounds != diff->flareSounds)
		for(auto &&flareSound : outfit->flareSounds)
			for(int i = 0; i < flareSound.second; ++i)
				writer.Write("flare sound", flareSound.first->Name());
	if(!diff || outfit->reverseFlareSounds != diff->reverseFlareSounds)
		for(auto &&reverseFlareSound : outfit->reverseFlareSounds)
			for(int i = 0; i < reverseFlareSound.second; ++i)
				writer.Write("reverse flare sound", reverseFlareSound.first->Name());
	if(!diff || outfit->steeringFlareSounds != diff->steeringFlareSounds)
		for(auto &&steeringFlareSound : outfit->steeringFlareSounds)
			for(int i = 0; i < steeringFlareSound.second; ++i)
				writer.Write("steering flare sound", steeringFlareSound.first->Name());
	if(!diff || outfit->hyperSounds != diff->hyperSounds)
		for(auto &&hyperdriveSound : outfit->hyperSounds)
			for(int i = 0; i < hyperdriveSound.second; ++i)
				writer.Write("hyperdrive sound", hyperdriveSound.first->Name());
	if(!diff || outfit->hyperInSounds != diff->hyperInSounds)
		for(auto &&hyperdriveInSound : outfit->hyperInSounds)
			for(int i = 0; i < hyperdriveInSound.second; ++i)
				writer.Write("hyperdrive in sound", hyperdriveInSound.first->Name());
	if(!diff || outfit->hyperOutSounds != diff->hyperOutSounds)
		for(auto &&hyperdriveOutSound : outfit->hyperOutSounds)
			for(int i = 0; i < hyperdriveOutSound.second; ++i)
				writer.Write("hyperdrive out sound", hyperdriveOutSound.first->Name());
	if(!diff || outfit->jumpSounds != diff->jumpSounds)
		for(auto &&jumpSound : outfit->jumpSounds)
			for(int i = 0; i < jumpSound.second; ++i)
				writer.Write("jump sound", jumpSound.first->Name());
	if(!diff || outfit->jumpInSounds != diff->jumpInSounds)
		for(auto &&jumpInSound : outfit->jumpInSounds)
			for(int i = 0; i < jumpInSound.second; ++i)
				writer.Write("jump in sound", jumpInSound.first->Name());
	if(!diff || outfit->jumpOutSounds != diff->jumpOutSounds)
		for(auto &&jumpOutSound : outfit->jumpOutSounds)
			for(int i = 0; i < jumpOutSound.second; ++i)
				writer.Write("jump out sound", jumpOutSound.first->Name());
	if(!diff || outfit->afterburnerEffects != diff->afterburnerEffects)
		for(auto &&afterburnerEffect : outfit->afterburnerEffects)
			for(int i = 0; i < afterburnerEffect.second; ++i)
				writer.Write("afterburner effect", afterburnerEffect.first->Name());
	if(!diff || outfit->jumpEffects != diff->jumpEffects)
		for(auto &&jumpEffect : outfit->jumpEffects)
			for(int i = 0; i < jumpEffect.second; ++i)
				writer.Write("jump effect", jumpEffect.first->Name());

	bool hasWrittenWeapon = false;
	auto writeWeapon = [&hasWrittenWeapon, &writer]()
	{
		if(!hasWrittenWeapon)
		{
			writer.Write("weapon");
			writer.BeginChild();
			hasWrittenWeapon = true; 
		}
	};

	if(!diff || outfit->isSafe != diff->isSafe)
		if(outfit->isSafe)
		{
			writeWeapon();
			writer.Write("safe");
		}
	if(!diff || outfit->isPhasing != diff->isPhasing)
		if(outfit->isPhasing)
		{
			writeWeapon();
			writer.Write("phasing");
		}
	if(!diff || outfit->isDamageScaled != diff->isDamageScaled)
		if(!outfit->isDamageScaled)
		{
			writeWeapon();
			writer.Write("no damage scaling");
		}
	if(!diff || outfit->isParallel != diff->isParallel)
		if(outfit->isParallel)
		{
			writeWeapon();
			writer.Write("parallel");
		}
	if(!diff || outfit->isGravitational != diff->isGravitational)
		if(outfit->isGravitational)
		{
			writeWeapon();
			writer.Write("gravitational");
		}
	if(!diff || outfit->sprite != outfit->sprite)
		if(outfit->sprite.HasSprite())
		{
			writeWeapon();
			outfit->sprite.SaveSprite(writer);
		}
	if(!diff || outfit->hardpointSprite != diff->hardpointSprite)
		if(outfit->hardpointSprite.HasSprite())
		{
			writeWeapon();
			outfit->hardpointSprite.SaveSprite(writer, "hardpoint sprite");
		}
	if(!diff || outfit->sound != diff->sound)
		if(outfit->sound)
		{
			writeWeapon();
			writer.Write("sound", outfit->sound->Name());
		}
	if(!diff || outfit->ammo != diff->ammo)
		if(outfit->ammo.first || diff)
		{
			writeWeapon();
			writer.WriteToken("ammo");
			writer.WriteToken(outfit->ammo.first->Name());
			if(outfit->ammo.second != 1)
				writer.WriteToken(outfit->ammo.second);
			writer.Write();
		}
	if(!diff || outfit->icon != diff->icon)
		if(outfit->icon)
		{
			writeWeapon();
			writer.Write("icon", outfit->icon->Name());
		}

	if(!diff || outfit->fireEffects != diff->fireEffects)
		if(!outfit->fireEffects.empty())
		{
			writeWeapon();
			for(auto &&fireEffect : outfit->fireEffects)
				if(fireEffect.second == 1)
					writer.Write("fire effect", fireEffect.first->Name());
				else
					writer.Write("fire effect", fireEffect.first->Name(), fireEffect.second);
		}
	if(!diff || outfit->liveEffects != diff->liveEffects)
		if(!outfit->liveEffects.empty())
		{
			writeWeapon();
			for(auto &&liveEffect : outfit->liveEffects)
				if(liveEffect.second == 1)
					writer.Write("live effect", liveEffect.first->Name());
				else
					writer.Write("live effect", liveEffect.first->Name(), liveEffect.second);
		}
	if(!diff || outfit->hitEffects != diff->hitEffects)
		if(!outfit->hitEffects.empty())
		{
			writeWeapon();
			for(auto &&hitEffect : outfit->hitEffects)
				if(hitEffect.second == 1)
					writer.Write("hit effect", hitEffect.first->Name());
				else
					writer.Write("hit effect", hitEffect.first->Name(), hitEffect.second);
		}
	if(!diff || outfit->targetEffects != diff->targetEffects)
		if(!outfit->targetEffects.empty())
		{
			writeWeapon();
			for(auto &&targetEffect : outfit->targetEffects)
				if(targetEffect.second == 1)
					writer.Write("target effect", targetEffect.first->Name());
				else
					writer.Write("target effect", targetEffect.first->Name(), targetEffect.second);
		}
	if(!diff || outfit->dieEffects != diff->dieEffects)
		if(!outfit->dieEffects.empty())
		{
			writeWeapon();
			for(auto &&dieEffect : outfit->dieEffects)
				if(dieEffect.second == 1)
					writer.Write("die effect", dieEffect.first->Name());
				else
					writer.Write("die effect", dieEffect.first->Name(), dieEffect.second);
		}

	if(!diff || outfit->submunitions != diff->submunitions)
		if(!outfit->submunitions.empty())
		{
			writeWeapon();
			for(auto &&submunition : outfit->submunitions)
			{
				writer.WriteToken("submunition");
				writer.WriteToken(submunition.weapon->Name());
				if(submunition.count > 1)
					writer.WriteToken(submunition.count);
				writer.Write();
				writer.BeginChild();
				if(submunition.facing.Degrees())
					writer.Write("facing", submunition.facing.Degrees());
				if(submunition.offset)
					writer.Write("offset", submunition.offset.X(), submunition.offset.Y());
				writer.EndChild();
			}
		}

	if(!diff || outfit->lifetime != diff->lifetime)
		if(outfit->lifetime || diff)
		{
			writeWeapon();
			writer.Write("lifetime", outfit->lifetime);
		}
	if(!diff || outfit->randomLifetime != diff->randomLifetime)
		if(outfit->randomLifetime || diff)
		{
			writeWeapon();
			writer.Write("random lifetime", outfit->randomLifetime);
		}
	if(!diff || outfit->reload != diff->reload)
		if(outfit->reload != 1. || diff)
		{
			writeWeapon();
			writer.Write("reload", outfit->reload);
		}
	if(!diff || outfit->burstReload != diff->burstReload)
		if(outfit->burstReload != 1. || diff)
		{
			writeWeapon();
			writer.Write("burst reload", outfit->burstReload);
		}
	if(!diff || outfit->burstCount != diff->burstCount)
		if(outfit->burstCount != 1 || diff)
		{
			writeWeapon();
			writer.Write("burst count", outfit->burstCount);
		}
	if(!diff || outfit->homing != diff->homing)
		if(outfit->homing || diff)
		{
			writeWeapon();
			writer.Write("homing", outfit->homing);
		}
	if(!diff || outfit->missileStrength != diff->missileStrength)
		if(outfit->missileStrength || diff)
		{
			writeWeapon();
			writer.Write("missile strength", outfit->missileStrength);
		}
	if(!diff || outfit->antiMissile != diff->antiMissile)
		if(outfit->antiMissile || diff)
		{
			writeWeapon();
			writer.Write("anti-missile", outfit->antiMissile);
		}
	if(!diff || outfit->velocity != diff->velocity)
		if(outfit->velocity || diff)
		{
			writeWeapon();
			writer.Write("velocity", outfit->velocity);
		}
	if(!diff || outfit->randomVelocity != diff->randomVelocity)
		if(outfit->randomVelocity || diff)
		{
			writeWeapon();
			writer.Write("random velocity", outfit->randomVelocity);
		}
	if(!diff || outfit->acceleration != diff->acceleration)
		if(outfit->acceleration || diff)
		{
			writeWeapon();
			writer.Write("acceleration", outfit->acceleration);
		}
	if(!diff || outfit->drag != diff->drag)
		if(outfit->drag || diff)
		{
			writeWeapon();
			writer.Write("drag", outfit->drag);
		}
	if(!diff || outfit->hardpointOffset != diff->hardpointOffset)
		if(outfit->hardpointOffset || diff)
		{
			writeWeapon();
			if(!outfit->hardpointOffset.X())
				writer.Write("hardpoint offset", -outfit->hardpointOffset.Y());
			else
				writer.Write("hardpoint offset", outfit->hardpointOffset.X(), -outfit->hardpointOffset.Y());
		}
	if(!diff || outfit->turn != diff->turn)
		if(outfit->turn || diff)
		{
			writeWeapon();
			writer.Write("turn", outfit->turn);
		}
	if(!diff || outfit->inaccuracy != diff->inaccuracy)
		if(outfit->inaccuracy || diff)
		{
			writeWeapon();
			writer.Write("inaccuracy", outfit->inaccuracy);
		}
	if(!diff || outfit->turretTurn != diff->turretTurn)
		if(outfit->turretTurn || diff)
		{
			writeWeapon();
			writer.Write("turret turn", outfit->turretTurn);
		}
	if(!diff || outfit->tracking != diff->tracking)
		if(outfit->tracking || diff)
		{
			writeWeapon();
			writer.Write("tracking", outfit->tracking);
		}
	if(!diff || outfit->opticalTracking != diff->opticalTracking)
		if(outfit->opticalTracking || diff)
		{
			writeWeapon();
			writer.Write("optical tracking", outfit->opticalTracking);
		}
	if(!diff || outfit->infraredTracking != diff->infraredTracking)
		if(outfit->infraredTracking || diff)
		{
			writeWeapon();
			writer.Write("infrared tracking", outfit->infraredTracking);
		}
	if(!diff || outfit->radarTracking != diff->radarTracking)
		if(outfit->radarTracking || diff)
		{
			writeWeapon();
			writer.Write("radar tracking", outfit->radarTracking);
		}
	if(!diff || outfit->firingEnergy != diff->firingEnergy)
		if(outfit->firingEnergy || diff)
		{
			writeWeapon();
			writer.Write("firing energy", outfit->firingEnergy);
		}
	if(!diff || outfit->firingForce != diff->firingForce)
		if(outfit->firingForce || diff)
		{
			writeWeapon();
			writer.Write("firing force", outfit->firingForce);
		}
	if(!diff || outfit->firingFuel != diff->firingFuel)
		if(outfit->firingFuel || diff)
		{
			writeWeapon();
			writer.Write("firing fuel", outfit->firingFuel);
		}
	if(!diff || outfit->firingHeat != diff->firingHeat)
		if(outfit->firingHeat || diff)
		{
			writeWeapon();
			writer.Write("firing heat", outfit->firingHeat);
		}
	if(!diff || outfit->firingHull != diff->firingHull)
		if(outfit->firingHull || diff)
		{
			writeWeapon();
			writer.Write("firing hull", outfit->firingHull);
		}
	if(!diff || outfit->firingShields != diff->firingShields)
		if(outfit->firingShields || diff)
		{
			writeWeapon();
			writer.Write("firing shields", outfit->firingShields);
		}
	if(!diff || outfit->firingIon != diff->firingIon)
		if(outfit->firingIon || diff)
		{
			writeWeapon();
			writer.Write("firing ion", outfit->firingIon);
		}
	if(!diff || outfit->firingSlowing != diff->firingSlowing)
		if(outfit->firingSlowing)
		{
			writeWeapon();
			writer.Write("firing slowing", outfit->firingSlowing);
		}
	if(!diff || outfit->firingDisruption != diff->firingDisruption)
		if(outfit->firingDisruption || diff)
		{
			writeWeapon();
			writer.Write("firing disruption", outfit->firingDisruption);
		}
	if(!diff || outfit->relativeFiringEnergy != diff->relativeFiringEnergy)
		if(outfit->relativeFiringEnergy || diff)
		{
			writeWeapon();
			writer.Write("relative firing energy", outfit->relativeFiringEnergy);
		}
	if(!diff || outfit->relativeFiringHeat != diff->relativeFiringHeat)
		if(outfit->relativeFiringHeat || diff)
		{
			writeWeapon();
			writer.Write("relative firing heat", outfit->relativeFiringHeat);
		}
	if(!diff || outfit->relativeFiringFuel != diff->relativeFiringFuel)
		if(outfit->relativeFiringFuel || diff)
		{
			writeWeapon();
			writer.Write("relative firing fuel", outfit->relativeFiringFuel);
		}
	if(!diff || outfit->relativeFiringHull != diff->relativeFiringHull)
		if(outfit->relativeFiringHull || diff)
		{
			writeWeapon();
			writer.Write("relative firing hull", outfit->relativeFiringHull);
		}
	if(!diff || outfit->relativeFiringShields != diff->relativeFiringShields)
		if(outfit->relativeFiringShields || diff)
		{
			writeWeapon();
			writer.Write("relative firing shields", outfit->relativeFiringShields);
		}
	if(!diff || outfit->splitRange != diff->splitRange)
		if(outfit->splitRange || diff)
		{
			writeWeapon();
			writer.Write("split range", outfit->splitRange);
		}
	if(!diff || outfit->triggerRadius != diff->triggerRadius)
		if(outfit->triggerRadius || diff)
		{
			writeWeapon();
			writer.Write("trigger radius", outfit->triggerRadius);
		}
	if(!diff || outfit->blastRadius != diff->blastRadius)
		if(outfit->blastRadius || diff)
		{
			writeWeapon();
			writer.Write("blast radius", outfit->blastRadius);
		}
	if(!diff || outfit->damage[Weapon::SHIELD_DAMAGE] != diff->damage[Weapon::SHIELD_DAMAGE])
		if(outfit->damage[Weapon::SHIELD_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("shield damage", outfit->damage[Weapon::SHIELD_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::HULL_DAMAGE] != diff->damage[Weapon::HULL_DAMAGE])
		if(outfit->damage[Weapon::HULL_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("hull damage", outfit->damage[Weapon::HULL_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::FUEL_DAMAGE] != diff->damage[Weapon::FUEL_DAMAGE])
		if(outfit->damage[Weapon::FUEL_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("fuel damage", outfit->damage[Weapon::FUEL_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::HEAT_DAMAGE] != diff->damage[Weapon::HEAT_DAMAGE])
		if(outfit->damage[Weapon::HEAT_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("heat damage", outfit->damage[Weapon::HEAT_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::ENERGY_DAMAGE] != diff->damage[Weapon::ENERGY_DAMAGE])
		if(outfit->damage[Weapon::ENERGY_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("energy damage", outfit->damage[Weapon::ENERGY_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::ION_DAMAGE] != diff->damage[Weapon::ION_DAMAGE])
		if(outfit->damage[Weapon::ION_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("ion damage", outfit->damage[Weapon::ION_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::DISRUPTION_DAMAGE] != diff->damage[Weapon::DISRUPTION_DAMAGE])
		if(outfit->damage[Weapon::DISRUPTION_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("disruption damage", outfit->damage[Weapon::DISRUPTION_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::SLOWING_DAMAGE] != diff->damage[Weapon::SLOWING_DAMAGE])
		if(outfit->damage[Weapon::SLOWING_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("slowing damage", outfit->damage[Weapon::SLOWING_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::RELATIVE_SHIELD_DAMAGE] != diff->damage[Weapon::RELATIVE_SHIELD_DAMAGE])
		if(outfit->damage[Weapon::RELATIVE_SHIELD_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("relative shield damage", outfit->damage[Weapon::RELATIVE_SHIELD_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::RELATIVE_HULL_DAMAGE] != diff->damage[Weapon::RELATIVE_HULL_DAMAGE])
		if(outfit->damage[Weapon::RELATIVE_HULL_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("relative hull damage", outfit->damage[Weapon::RELATIVE_HULL_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::RELATIVE_FUEL_DAMAGE] != diff->damage[Weapon::RELATIVE_FUEL_DAMAGE])
		if(outfit->damage[Weapon::RELATIVE_FUEL_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("relative fuel damage", outfit->damage[Weapon::RELATIVE_FUEL_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::RELATIVE_HEAT_DAMAGE] != diff->damage[Weapon::RELATIVE_HEAT_DAMAGE])
		if(outfit->damage[Weapon::RELATIVE_HEAT_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("relative heat damage", outfit->damage[Weapon::RELATIVE_HEAT_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::RELATIVE_ENERGY_DAMAGE] != diff->damage[Weapon::RELATIVE_ENERGY_DAMAGE])
		if(outfit->damage[Weapon::RELATIVE_ENERGY_DAMAGE] || diff)
		{
			writeWeapon();
			writer.Write("relative energy damage", outfit->damage[Weapon::RELATIVE_ENERGY_DAMAGE]);
		}
	if(!diff || outfit->damage[Weapon::HIT_FORCE] != diff->damage[Weapon::HIT_FORCE])
		if(outfit->damage[Weapon::HIT_FORCE] || diff)
		{
			writeWeapon();
			writer.Write("hit force", outfit->damage[Weapon::HIT_FORCE]);
		}
	if(!diff || outfit->piercing != diff->piercing)
		if(outfit->piercing || diff)
		{
			writeWeapon();
			writer.Write("piercing", outfit->piercing);
		}
	if(!diff || outfit->rangeOverride != diff->rangeOverride)
		if(outfit->rangeOverride || diff)
		{
			writeWeapon();
			writer.Write("range override", outfit->rangeOverride);
		}
	if(!diff || outfit->velocityOverride != diff->velocityOverride)
		if(outfit->velocityOverride || diff)
		{
			writeWeapon();
			writer.Write("velocity override", outfit->velocityOverride);
		}
	if(!diff || outfit->damageDropoffRange != diff->damageDropoffRange)
		if(outfit->hasDamageDropoff || diff)
		{
			writeWeapon();
			writer.WriteToken("damage dropoff");
			writer.WriteToken(outfit->damageDropoffRange.first);
			if(outfit->damageDropoffRange.second != outfit->Range())
				writer.WriteToken(outfit->damageDropoffRange.second);
			writer.Write();
		}
	if(!diff || outfit->damageDropoffModifier != diff->damageDropoffModifier)
		if(outfit->damageDropoffModifier || diff)
		{
			writeWeapon();
			writer.Write("dropoff modifier", outfit->damageDropoffModifier);
		}

	if((!diff || outfit->isStreamed != diff->isStreamed) && hasWrittenWeapon)
	{
		if((outfit->MissileStrength() || outfit->AntiMissile()) && outfit->isStreamed)
		{
			writeWeapon();
			writer.Write("stream");
		}
		if(!outfit->MissileStrength() && !outfit->AntiMissile() && !outfit->isStreamed)
		{
			writeWeapon();
			writer.Write("cluster");
		}
	}
	if(hasWrittenWeapon)
		writer.EndChild();

	if(!diff || outfit->description != diff->description)
		if(!outfit->description.empty())
		{
			size_t newline = outfit->description.find('\n');
			size_t start = 0;
			do {
				string toWrite = outfit->description.substr(start, newline - start);
				if(toWrite.empty())
					break;
				writer.Write("description", toWrite);

				start = newline + 1;
				newline = outfit->description.find('\n', start);
			} while(newline != string::npos);
		}
	writer.EndChild();
}
