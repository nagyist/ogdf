/** \file
 * \brief Declaration of interface for energy-based layout algorithms
 * (class ForceLayoutModule)
 *
 * \author Karsten Klein
 *
 * \par License:
 * This file is part of the Open Graph Drawing Framework (OGDF).
 *
 * \par
 * Copyright (C)<br>
 * See README.md in the OGDF root directory for details.
 *
 * \par
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * Version 2 or 3 as published by the Free Software Foundation;
 * see the file LICENSE.txt included in the packaging of this file
 * for details.
 *
 * \par
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * \par
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see
 * http://www.gnu.org/copyleft/gpl.html
 */

#pragma once

#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/basic/LayoutModule.h>
#include <ogdf/basic/basic.h>
#include <ogdf/basic/memory.h>
#include <ogdf/energybased/multilevel_mixer/MultilevelGraph.h>

namespace ogdf {

/**
 * \brief Interface of general layout algorithms.
 *
 */
class OGDF_EXPORT ForceLayoutModule : public LayoutModule {
	// holds index of the current level in multilevel hierarchy
	int m_currentLevel;

public:
	//! Initializes a force layout module.
	ForceLayoutModule() { }

	virtual ~ForceLayoutModule() { }

	virtual void call(GraphAttributes& GA) override = 0;

	/**
	 * \brief Computes a layout of graph \a MLG.
	 *
	 * This method can be implemented optionally to allow a LayoutModule to modify the Graph.
	 * This allows some Layout Algorithms to save Memory, compared to a normal call(GA)
	 * DO NOT implement this if you are not sure whether this would save you Memory!
	 * This Method only helps if the Graph is already in the MultiLevelGraph Format
	 *   (or can be converted without creating a copy) AND the Layout would need a copy otherwise.
	 * All incremental Layouts (especially energy based) CAN be called by ModularMultilevelMixer.
	 * The standard implementation converts the MLG to GA and uses call(GA).
	 *
	 * If implemented, the following Implementation of call(GA) is advised
	 *   to ensure consistent behaviour of the two call Methods:
	 * void YourLayout::call(GraphAttributes &GA)
	 * {
	 *     MultilevelGraph MLG(GA);
	 *     call(MLG);
	 *     MLG.exportAttributes(GA);
	 * }
	 *
	 * @param MLG is the input graph and will also be assigned the layout information.
	 */
	virtual void call(MultilevelGraph& MLG) {
		m_currentLevel = MLG.getLevel();
		GraphAttributes GA(MLG.getGraph());
		MLG.exportAttributesSimple(GA);
		call(GA);
		MLG.importAttributesSimple(GA);
	};

	OGDF_MALLOC_NEW_DELETE
};

}
