/** \file
 * \brief Declaration and Implementation of class PlanarSubgraphEmpty.
 * Heuristic: We obtain a planar subgraph by putting all edges in \a delEdges.
 *
 * \author Tilo Wiedera
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

#include <ogdf/basic/Graph.h>
#include <ogdf/basic/Module.h>
#include <ogdf/planarity/PlanarSubgraphModule.h>

namespace ogdf {
template<class E>
class List;

//! Dummy implementation for maximum planar subgraph that returns an empty graph.
/**
 * @ingroup ga-plansub
 */
template<typename TCost>
class PlanarSubgraphEmpty : public PlanarSubgraphModule<TCost> {
public:
	virtual PlanarSubgraphEmpty* clone() const override { return new PlanarSubgraphEmpty(); }

	virtual Module::ReturnType doCall(const Graph& graph, const List<edge>& preferredEdges,
			List<edge>& delEdges, const EdgeArray<TCost>* pCost, bool preferedImplyPlanar) override {
		graph.allEdges(delEdges);
		return Module::ReturnType::Feasible;
	}
};
}
