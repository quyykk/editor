/* SystemEditor.h
Copyright (c) 2021 quyykk

Endless Sky is free software: you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation, either version 3 of the License, or (at your option) any later version.

Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.  See the GNU General Public License for more details.
*/

#ifndef SYSTEM_EDITOR_H_
#define SYSTEM_EDITOR_H_

#include "System.h"
#include "TemplateEditor.h"

#include <set>
#include <string>
#include <list>

class DataWriter;
class Editor;
class MainEditorPanel;
class MapEditorPanel;
class StellarObject;



// Class representing the system editor window.
class SystemEditor : public TemplateEditor<System> {
public:
	SystemEditor(Editor &editor, bool &show) noexcept;

	void Render();
	void WriteToFile(DataWriter &writer, const System *system);

	// Updates the given system's position by the given delta.
	void UpdateSystemPosition(const System *system, Point dp);
	// Updates the given stellar's position by the given delta.
	void UpdateStellarPosition(const StellarObject &object, Point dp, const System *system);
	// Toggles a link between the current object and the given one.
	void ToggleLink(const System *system);
	// Create a new system at the specified position.
	void CreateNewSystem(Point position);

	const System *Selected() const { return object; }
	void Select(const System *system) { object = const_cast<System *>(system); }


private:
	void RenderSystem();
	void RenderObject(StellarObject &object, int index, int &nested, bool &hovered, bool &add);

	void WriteObject(DataWriter &writer, const System *system, const StellarObject *object, bool add = false);

	void UpdateMap() const;
	void UpdateAsteroids() const;

	void Randomize();
	void RandomizeAsteroids();
	void RandomizeMinables();

	const Sprite *RandomStarSprite();
	const Sprite *RandomPlanetSprite(bool recalculate = false);
	const Sprite *RandomMoonSprite();
	const Sprite *RandomGiantSprite();


private:
	Point position;
	bool createNewSystem = false;
};



#endif
