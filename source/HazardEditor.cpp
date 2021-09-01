/* HazardEditor.cpp
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#include "HazardEditor.h"

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

#include <cassert>
#include <map>

using namespace std;



HazardEditor::HazardEditor(Editor &editor, bool &show) noexcept
	: TemplateEditor<Hazard>(editor, show)
{
}



void HazardEditor::Render()
{
	if(IsDirty())
	{
		ImGui::PushStyleColor(ImGuiCol_TitleBg, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, static_cast<ImVec4>(ImColor(255, 91, 71)));
		ImGui::PushStyleColor(ImGuiCol_TitleBgCollapsed, static_cast<ImVec4>(ImColor(255, 91, 71)));
	}

	ImGui::SetNextWindowSize(ImVec2(550, 500), ImGuiCond_FirstUseEver);
	if(!ImGui::Begin("Hazard Editor", &show))
	{
		if(IsDirty())
			ImGui::PopStyleColor(3);
		ImGui::End();
		return;
	}

	if(IsDirty())
		ImGui::PopStyleColor(3);

	if(ImGui::BeginCombo("hazard", object ? object->Name().c_str() : ""))
	{
		for(const auto &hazard : GameData::Hazards())
		{
			const bool selected = object ? object->Name() == hazard.first : false;
			if(ImGui::Selectable(hazard.first.c_str(), selected))
				object = const_cast<Hazard *>(&hazard.second);
			if(selected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}
	ImGui::InputText("new clone name:", &searchBox);

	if(!object || !IsDirty())
		ImGui::PushDisabled();
	bool reset = ImGui::Button("Reset");
	if(!object || !IsDirty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!object)
				ImGui::SetTooltip("Select a hazard first.");
			else if(!IsDirty())
				ImGui::SetTooltip("No changes to reset.");
		}
	}
	ImGui::SameLine();
	if(!object || searchBox.empty())
		ImGui::PushDisabled();
	bool clone = ImGui::Button("Clone");
	if(!object || searchBox.empty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(searchBox.empty())
				ImGui::SetTooltip("Input the new name for the hazard above.");
			else if(!object)
				ImGui::SetTooltip("Select a hazard first.");
		}
	}
	ImGui::SameLine();
	if(!object || !editor.HasPlugin() || !IsDirty())
		ImGui::PushDisabled();
	bool save = ImGui::Button("Save");
	if(!object || !editor.HasPlugin() || !IsDirty())
	{
		ImGui::PopDisabled();
		if(ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
		{
			if(!object)
				ImGui::SetTooltip("Select a hazard first.");
			else if(!editor.HasPlugin())
				ImGui::SetTooltip("Load a plugin to save to a file.");
			else if(!IsDirty())
				ImGui::SetTooltip("No changes to save.");
		}
	}

	if(!object)
	{
		ImGui::End();
		return;
	}

	if(reset)
	{
		bool found = false;
		for(auto &&change : Changes())
			if(change.Name() == object->Name())
			{
				*object = change;
				found = true;
				break;
			}
		if(!found)
			*object = *GameData::baseHazards.Get(object->name);
		SetClean();
	}
	if(clone)
	{
		auto *clone = const_cast<Hazard *>(GameData::Hazards().Get(searchBox));
		*clone = *object;
		object = clone;

		object->name = searchBox;
		searchBox.clear();
		SetDirty();
	}
	if(save)
		WriteToPlugin(object);

	ImGui::Separator();
	ImGui::Spacing();
	RenderHazard();
	ImGui::End();
}



void HazardEditor::RenderHazard()
{
	bool constantStrength = !object->deviates;
	if(ImGui::Checkbox("constant strength", &constantStrength))
	{
		object->deviates = !constantStrength;
		SetDirty();
	}
	if(ImGui::InputInt("period", &object->period))
	{
		object->period = max(1, object->period);
		SetDirty();
	}
	int durations[2] = {object->minDuration, object->maxDuration};
	if(ImGui::InputInt2("duration", durations))
	{
		object->minDuration = max(0, durations[0]);
		object->maxDuration = max(object->minDuration, durations[1]);
		SetDirty();
	}
	double strengths[2] = {object->minStrength, object->maxStrength};
	if(ImGui::InputDouble2Ex("strength", strengths))
	{
		object->minStrength = max(0., strengths[0]);
		object->maxStrength = max(object->minStrength, strengths[1]);
		SetDirty();
	}
	double ranges[2] = {object->minRange, object->maxRange};
	if(ImGui::InputDouble2Ex("range", ranges))
	{
		object->minRange = max(0., ranges[0]);
		object->maxRange = max(object->minRange, ranges[1]);
		SetDirty();
	}

	if(ImGui::TreeNode("environmental effects"))
	{
		int index = 0;
		auto oldEffect = object->environmentalEffects.end();
		const Effect *newEffect = nullptr;
		for(auto it = object->environmentalEffects.begin(); it != object->environmentalEffects.end(); ++it)
		{
			ImGui::PushID(index++);
			string effectName = it->first ? it->first->Name() : "";
			if(ImGui::BeginCombo("effects", effectName.c_str()))
			{
				for(const auto &effect : GameData::Effects())
				{
					const bool selected = &effect.second == it->first;
					if(ImGui::Selectable(effect.first.c_str(), selected))
					{
						oldEffect = it;
						newEffect = &effect.second;
						SetDirty();
					}
					if(selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();
			if(ImGui::InputInt("##count", &it->second))
			{
				if(!it->second)
					oldEffect = it;
				SetDirty();
			}
			ImGui::PopID();
		}
		if(oldEffect != object->environmentalEffects.end())
		{
			int count = oldEffect->second;
			object->environmentalEffects.erase(oldEffect);
			if(newEffect)
				object->environmentalEffects[newEffect] += count;
		}

		ImGui::Spacing();
		if(ImGui::BeginCombo("add effect", ""))
		{
			const Effect *toAdd = nullptr;
			for(const auto &effect : GameData::Effects())
				if(ImGui::Selectable(effect.first.c_str()))
					toAdd = &effect.second;

			if(toAdd)
			{
				++object->environmentalEffects[toAdd];
				SetDirty();
			}
			ImGui::EndCombo();
		}
		ImGui::TreePop();
	}

	if(ImGui::TreeNode("weapon"))
	{
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
		if(ImGui::InputDoubleEx("relative shield damage", &object->damage[Weapon::RELATIVE_SHIELD_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative hull damage", &object->damage[Weapon::RELATIVE_HULL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative hull damage", &object->damage[Weapon::RELATIVE_FUEL_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("relative heat damage", &object->damage[Weapon::RELATIVE_HEAT_DAMAGE]))
			SetDirty();
		if(ImGui::InputDoubleEx("piecing", &object->piercing))
		{
			object->piercing = max(0., object->piercing);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("hit force", &object->damage[Weapon::HIT_FORCE]))
			SetDirty();
		if(ImGui::Checkbox("gravitational", &object->isGravitational))
			SetDirty();
		if(ImGui::InputDoubleEx("blast radius", &object->blastRadius))
		{
			object->blastRadius = max(0., object->blastRadius);
			SetDirty();
		}
		if(ImGui::InputDoubleEx("trigger radius", &object->triggerRadius))
		{
			object->triggerRadius = max(0., object->triggerRadius);
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
		RenderEffect("target effect", object->targetEffects);
		ImGui::TreePop();
	}
}



void HazardEditor::WriteToFile(DataWriter &writer, const Hazard *hazard)
{
	const auto *diff = GameData::baseHazards.Has(hazard->name)
		? GameData::baseHazards.Get(hazard->name)
		: nullptr;

	writer.Write("hazard", hazard->Name());
	writer.BeginChild();

	if(!diff || hazard->deviates != diff->deviates)
		if(!hazard->deviates)
			writer.Write("constant strength");
	if(!diff || hazard->period != diff->period)
		if(hazard->period > 1 || diff)
			writer.Write("period", hazard->period);
	if(!diff || hazard->minDuration != diff->minDuration || hazard->maxDuration != diff->maxDuration)
		if(hazard->minDuration > 1 || hazard->maxDuration > 1 || diff)
		{
			writer.WriteToken("duration");
			writer.WriteToken(hazard->minDuration);
			if(hazard->maxDuration > 1 || (diff && hazard->maxDuration != diff->maxDuration))
				writer.WriteToken(hazard->maxDuration);
			writer.Write();
		}
	if(!diff || hazard->minStrength != diff->minStrength || hazard->maxStrength != diff->maxStrength)
		if(hazard->minStrength > 1. || hazard->maxStrength > 1. || diff)
		{
			writer.WriteToken("strength");
			writer.WriteToken(hazard->minStrength);
			if(hazard->maxStrength > 1. || (diff && hazard->maxStrength != diff->maxStrength))
				writer.WriteToken(hazard->maxStrength);
			writer.Write();
		}
	if(!diff || hazard->minRange != diff->minRange || hazard->maxRange != diff->maxRange)
		if(hazard->minRange > 0. || hazard->maxRange < 10000. || diff)
		{
			writer.WriteToken("range");
			if(hazard->minRange > 0. || (diff && hazard->minRange != diff->minRange))
				writer.WriteToken(hazard->minRange);
			writer.WriteToken(hazard->maxRange);
			writer.Write();
		}
	if(!diff || hazard->environmentalEffects != diff->environmentalEffects)
		for(const auto &pair : hazard->environmentalEffects)
			writer.Write("environmental effect", pair.first->Name(), pair.second);

	bool hasWrittenRoot = false;
	auto writeRoot = [&hasWrittenRoot, &writer]
	{
		if(!hasWrittenRoot)
		{
			writer.Write("weapon");
			writer.BeginChild();
			hasWrittenRoot = true;
		}
	};
	if(!diff || hazard->damage[Weapon::SHIELD_DAMAGE] != diff->damage[Weapon::SHIELD_DAMAGE])
		if(hazard->damage[Weapon::SHIELD_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("shield damage", hazard->damage[Weapon::SHIELD_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::HULL_DAMAGE] != diff->damage[Weapon::HULL_DAMAGE])
		if(hazard->damage[Weapon::HULL_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("hull damage", hazard->damage[Weapon::HULL_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::FUEL_DAMAGE] != diff->damage[Weapon::FUEL_DAMAGE])
		if(hazard->damage[Weapon::FUEL_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("fuel damage", hazard->damage[Weapon::FUEL_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::HEAT_DAMAGE] != diff->damage[Weapon::HEAT_DAMAGE])
		if(hazard->damage[Weapon::HEAT_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("heat damage", hazard->damage[Weapon::HEAT_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::ENERGY_DAMAGE] != diff->damage[Weapon::ENERGY_DAMAGE])
		if(hazard->damage[Weapon::ENERGY_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("energy damage", hazard->damage[Weapon::ENERGY_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::ION_DAMAGE] != diff->damage[Weapon::ION_DAMAGE])
		if(hazard->damage[Weapon::ION_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("ion damage", hazard->damage[Weapon::ION_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::DISRUPTION_DAMAGE] != diff->damage[Weapon::DISRUPTION_DAMAGE])
		if(hazard->damage[Weapon::DISRUPTION_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("disruption damage", hazard->damage[Weapon::DISRUPTION_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::SLOWING_DAMAGE] != diff->damage[Weapon::SLOWING_DAMAGE])
		if(hazard->damage[Weapon::SLOWING_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("slowing damage", hazard->damage[Weapon::SLOWING_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_SHIELD_DAMAGE] != diff->damage[Weapon::RELATIVE_SHIELD_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_SHIELD_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative shield damage", hazard->damage[Weapon::RELATIVE_SHIELD_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_HULL_DAMAGE] != diff->damage[Weapon::RELATIVE_HULL_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_HULL_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative hull damage", hazard->damage[Weapon::RELATIVE_HULL_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_FUEL_DAMAGE] != diff->damage[Weapon::RELATIVE_FUEL_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_FUEL_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative fuel damage", hazard->damage[Weapon::RELATIVE_FUEL_DAMAGE]);
		}
	if(!diff || hazard->damage[Weapon::RELATIVE_HEAT_DAMAGE] != diff->damage[Weapon::RELATIVE_HEAT_DAMAGE])
		if(hazard->damage[Weapon::RELATIVE_HEAT_DAMAGE] || diff)
		{
			writeRoot();
			writer.Write("relative heat damage", hazard->damage[Weapon::RELATIVE_HEAT_DAMAGE]);
		}
	if(!diff || hazard->piercing != diff->piercing)
		if(hazard->piercing || diff)
		{
			writeRoot();
			writer.Write("piercing", hazard->piercing);
		}
	if(!diff || hazard->damage[Weapon::HIT_FORCE] != diff->damage[Weapon::HIT_FORCE])
		if(hazard->damage[Weapon::HIT_FORCE] || diff)
		{
			writeRoot();
			writer.Write("hit force", hazard->damage[Weapon::HIT_FORCE]);
		}
	if(!diff || hazard->isGravitational != diff->isGravitational)
		if(hazard->isGravitational || diff)
		{
			writeRoot();
			writer.Write("gravitational");
		}
	if(!diff || hazard->blastRadius != diff->blastRadius)
		if(hazard->blastRadius || diff)
		{
			writeRoot();
			writer.Write("blast radius", hazard->blastRadius);
		}
	if(!diff || hazard->damageDropoffRange != diff->damageDropoffRange)
		if(hazard->damageDropoffRange.first || hazard->damageDropoffRange.second || diff)
		{
			writeRoot();
			writer.Write("damage dropoff", hazard->damageDropoffRange.first, hazard->damageDropoffRange.second);
		}
	if(!diff || hazard->damageDropoffModifier != diff->damageDropoffModifier)
		if(hazard->damageDropoffModifier || diff)
		{
			writeRoot();
			writer.Write("dropoff modifier", hazard->damageDropoffModifier);
		}
	if(!diff || hazard->targetEffects != diff->targetEffects)
		if(!hazard->targetEffects.empty())
		{
			writeRoot();
			for(auto &&targetEffect : hazard->targetEffects)
				writer.Write("target effect", targetEffect.first->Name(), targetEffect.second);
		}
	if(hasWrittenRoot)
		writer.EndChild();
	writer.EndChild();
}
