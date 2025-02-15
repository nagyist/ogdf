/** \file
 * \brief implementation of PlanRepUML class
 *
 * \author Carsten Gutwenger
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


#include <ogdf/basic/Array.h>
#include <ogdf/basic/CombinatorialEmbedding.h>
#include <ogdf/basic/Graph.h>
#include <ogdf/basic/GraphAttributes.h>
#include <ogdf/basic/GraphList.h>
#include <ogdf/basic/GridLayoutMapped.h>
#include <ogdf/basic/Layout.h>
#include <ogdf/basic/List.h>
#include <ogdf/basic/SList.h>
#include <ogdf/basic/basic.h>
#include <ogdf/basic/tuples.h>
#include <ogdf/orthogonal/OrthoRep.h>
#include <ogdf/planarity/PlanRep.h>
#include <ogdf/uml/PlanRepUML.h>
#include <ogdf/uml/UMLGraph.h>

#include <fstream>
#include <string>

namespace ogdf {

PlanRepUML::PlanRepUML(const UMLGraph& umlGraph) : PlanRep(umlGraph), m_pUmlGraph(&umlGraph) {
	m_alignUpward.init(*this, false);
	m_faceSplitter.init(*this, false);
	m_incMergers.init(m_ccInfo.numberOfCCs());
}

PlanRepUML::PlanRepUML(const GraphAttributes& GA) : PlanRep(GA), m_pUmlGraph(nullptr) {
	m_alignUpward.init(*this, false);
	m_faceSplitter.init(*this, false);
	m_incMergers.init(m_ccInfo.numberOfCCs());
}

void PlanRepUML::initCC(int i) {
	PlanRep::initCC(i);

	if (m_pUmlGraph != nullptr) {
		//the new types that will replace the types just set
		//maybe this should not be executed in initcc, only in constr.
		//check this for crossings
		for (edge e : edges) {
			if (original(e)) {
				//edges should be embedded at outgoing generalization to allow alignment
				if (m_pUmlGraph->upwards(original(e)->adjSource())) {
					m_alignUpward[e->adjSource()] = true;
				} else {
					m_alignUpward[e->adjSource()] = false;
				}

				//maybe it's enough to set gen/ass without extra array
				//due to planarization we have to assure that no types are lost
				oriEdgeTypes(original(e)) = edgeTypes(e);
			}
		}
	}
}

void PlanRepUML::expand(bool lowDegreeExpand) {
	OGDF_ASSERT(representsCombEmbedding());

	for (node v : nodes) {
		// Replace merge vertices by cages.
		if (typeOf(v) == Graph::NodeType::generalizationMerger) {
			// Scan the list of edges of v to find the outgoing edge of v
			// Get then the cirular list of ingoing edges corresponding to
			// the planar embedding.
			SList<edge> inGens;
			bool detect = false;
			for (adjEntry adj : v->adjEntries) {
				edge e = adj->theEdge();
				OGDF_ASSERT(typeOf(e) == Graph::EdgeType::generalization);
				if (e->target() != v) {
					detect = true;
					continue;
				}
				if (detect) {
					inGens.pushBack(e);
				}
			}
			for (adjEntry adj : v->adjEntries) {
				edge e = adj->theEdge();
				if (e->target() != v) {
					break;
				}
				inGens.pushBack(e);
			}

			setExpandedNode(v, v);
			// Create the list of generalization expanders
			// We need degree(v)-1 of them to construct a face.
			SListPure<node> expander;
			for (int i = 0; i < v->degree() - 1; i++) {
				node u = newNode();
				typeOf(u) = Graph::NodeType::generalizationExpander;
				setExpandedNode(u, v);
				expander.pushBack(u);
			}

			// We move the target node of each ingoing generalization of v to a new
			// node stored in expander.
			// Note that, for each such edge e, the target node of the original
			// edge is then different from the original of the target node of e
			// (the latter is 0 because u is a new (dummy) node)
			SListConstIterator<edge> it;
			SListConstIterator<node> itn;
			NodeArray<adjEntry> ar(*this);

			itn = expander.begin();

			for (it = inGens.begin(); it.valid(); ++it) {
				// all edges in the list inGens must be ingoing generalizations of v
				OGDF_ASSERT((*it)->target() == v);
				OGDF_ASSERT(typeOf(*it) == Graph::EdgeType::generalization);
				OGDF_ASSERT(itn.valid());

				moveTarget(*it, *itn);
				ar[*itn] = (*itn)->firstAdj();
				++itn;
			}
			ar[v] = v->firstAdj();

			// Now introduce the circular list of new edges
			// forming the border of the merge face. Keep the embedding.
			adjEntry adjPrev = v->firstAdj();

			for (itn = expander.begin(); itn.valid(); ++itn) {
				edge e = newEdge(adjPrev, (*itn)->firstAdj());

				setExpansion(e);
				setGeneralization(e);

				if (!expandAdj(v)) {
					expandAdj(v) = e->adjSource();
				}

				adjPrev = (*itn)->firstAdj();
			}

			edge e = newEdge(adjPrev, v->lastAdj());

			setExpansion(e);
			setGeneralization(e);

			OGDF_ASSERT(representsCombEmbedding());
		}

		// Replace vertices with high degree by cages and
		// replace degree 4 vertices with two generalizations
		// adjacent in the embedding list by a cage.
		else if (v->degree() >= 4 && typeOf(v) != Graph::NodeType::dummy && !lowDegreeExpand) {
			// Check first how many generalizations there are.
			// If the node has degree 4 and at most one generalization
			// nothing is to do.
			int detect = 0;
			for (adjEntry adj : v->adjEntries) {
				edge e = adj->theEdge();
				if (!detect && typeOf(e) == Graph::EdgeType::generalization) {
					detect = 1;
				} else if (typeOf(e) == Graph::EdgeType::generalization) {
					detect = 2;
				}
			}
			if (v->degree() == 4 && detect < 2) {
				continue; // Nothing to do.
			}

			// Collects the nodes in the expanded face that
			// are adjacent to a generalization.
			// There are at most two of them.
			SList<node> genNodes;

			//Set the type of the node v. It remains in the graph
			// as one of the nodes of the expanded face.
			typeOf(v) = Graph::NodeType::highDegreeExpander;

			// Scann the list of edges of v to find the adjacent edges of v
			// according to the planar embedding. All except one edge
			// will get a new adjacent node
			// the planar embedding.
			SList<edge> adjEdges;
			for (adjEntry adj : v->adjEntries) {
				edge e = adj->theEdge();
				adjEdges.pushBack(e);
			}

			//The first edge remains at v. remove it from the list.
			// Check if it is a generalization.
			edge e = adjEdges.popFrontRet();

			//super sink check: don't use sink generalization, as the node may be deleted
			while (typeOf(e) == Graph::EdgeType::generalization) {
				adjEdges.pushBack(e);
				e = adjEdges.popFrontRet();
			}
			//check end

			if (typeOf(e) == Graph::EdgeType::generalization) {
				genNodes.pushBack(v);
			}

			// The node has maximum two generalization edges.
			OGDF_ASSERT(genNodes.size() <= 2);

			// Create the list of high degree expanders
			// We need degree(v)-1 of them to construct a face.
			// and set expanded Node to v
			setExpandedNode(v, v);

			SListPure<node> expander;
			for (int i = 0; i < v->degree() - 1; i++) {
				node u = newNode();
				typeOf(u) = Graph::NodeType::highDegreeExpander;
				setExpandedNode(u, v);
				expander.pushBack(u);
			}

			// We move the target node of each ingoing generalization of v to a new
			// node stored in expander.
			// Note that, for each such edge e, the target node of the original
			// edge is then different from the original of the target node of e
			// (the latter is 0 because u is a new (dummy) node)
			SListConstIterator<edge> it;
			SListConstIterator<node> itn;

			NodeArray<adjEntry> ar(*this);

			itn = expander.begin();

			for (it = adjEdges.begin(); it.valid(); ++it) {
				// Did we allocate enough dummy nodes?
				OGDF_ASSERT(itn.valid());

				// Check if edge is a generalization
				if (typeOf((*it)) == Graph::EdgeType::generalization) {
					genNodes.pushBack(*itn);
				}

				if ((*it)->source() == v) {
					moveSource(*it, *itn);
				} else {
					moveTarget(*it, *itn);
				}
				ar[*itn] = (*itn)->firstAdj();
				++itn;
			}
			ar[v] = v->firstAdj();


			// There may be at most two generalizations adjacent to v.
			OGDF_ASSERT(genNodes.size() <= 2);

			// Now introduce the circular list of new edges
			// forming the border of the merge face. Keep the embedding.
			adjEntry adjPrev = v->firstAdj();

			for (itn = expander.begin(); itn.valid(); ++itn) {
				e = newEdge(adjPrev, (*itn)->firstAdj());
				setExpansionEdge(e, 2); //can be removed if edgetypes work properly

				setExpansion(e);
				setAssociation(e);

				typeOf(e) = EdgeType::association; //???

				if (!expandAdj(v)) {
					expandAdj(v) = e->adjSource();
				}
				adjPrev = (*itn)->firstAdj();
			}

			e = newEdge(adjPrev, v->lastAdj());

			typeOf(e) = EdgeType::association; //???
			setExpansionEdge(e, 2); //can be removed if edgetypes work properly
			setAssociation(e);


			if (genNodes.size() == 2) {
				node u = genNodes.popFrontRet();
				node w = genNodes.popFrontRet();
				e = newEdge(u->firstAdj()->succ(), w->firstAdj()->succ());
				m_faceSplitter[e] = true;
			}

			OGDF_ASSERT(representsCombEmbedding());
		}

		// Replace all vertices with degree > 2 by cages.
		else if (v->degree() >= 2 && typeOf(v) != Graph::NodeType::dummy && lowDegreeExpand) {
			// Check first how many generalizations there are.
			// If the node has degree 4 and at most one generalization
			// nothing is to do.
			int detect = 0;
			for (adjEntry adj : v->adjEntries) {
				edge e = adj->theEdge();
				if (!detect && typeOf(e) == Graph::EdgeType::generalization) {
					detect = 1;
				} else if (typeOf(e) == Graph::EdgeType::generalization) {
					detect = 2;
				}
			}
#if 0
			if (v->degree() == 4 && detect < 2) {
				continue;  // Nothing to do.
			}
#endif

			// Collects the nodes in the expanded face that
			// are adjacent to a generalization.
			// There are at most two of them.
			SList<node> genNodes;

			//Set the type of the node v. It remains in the graph
			// as one of the nodes of the expanded face.
			typeOf(v) = Graph::NodeType::highDegreeExpander;

			// Scan the list of edges of v to find the adjacent edges of v
			// according to the planar embedding. All except one edge
			// will get a new adjacent node
			// the planar embedding.
			SList<edge> adjEdges;
			for (adjEntry adj : v->adjEntries) {
				edge e = adj->theEdge();
				adjEdges.pushBack(e);
			}

			//The first edge remains at v. remove it from the list.
			// Check if it is a generalization.
			edge e = adjEdges.popFrontRet();
			if (typeOf(e) == Graph::EdgeType::generalization) {
				genNodes.pushBack(v);
			}

			// The node has maximum two generalization edges.
			OGDF_ASSERT(genNodes.size() <= 2);

			// Create the list of high degree expanders
			// We need degree(v)-1 of them to construct a face.
			// and set expanded Node to v
			setExpandedNode(v, v);
			SListPure<node> expander;
			for (int i = 0; i < v->degree() - 1; i++) {
				node u = newNode();
				typeOf(u) = Graph::NodeType::highDegreeExpander;
				setExpandedNode(u, v);
				expander.pushBack(u);
			}


			// We move the target node of each ingoing generalization of v to a new
			// node stored in expander.
			// Note that, for each such edge e, the target node of the original
			// edge is then different from the original of the target node of e
			// (the latter is 0 because u is a new (dummy) node)
			SListConstIterator<edge> it;
			SListConstIterator<node> itn;

			NodeArray<adjEntry> ar(*this);

			itn = expander.begin();

			for (it = adjEdges.begin(); it.valid(); ++it) {
				// Did we allocate enough dummy nodes?
				OGDF_ASSERT(itn.valid());

				// Check if edge is a generalization
				if (typeOf((*it)) == Graph::EdgeType::generalization) {
					genNodes.pushBack(*itn);
				}

				if ((*it)->source() == v) {
					moveSource(*it, *itn);
				} else {
					moveTarget(*it, *itn);
				}
				ar[*itn] = (*itn)->firstAdj();
				++itn;
			}
			ar[v] = v->firstAdj();


			// There may be at most two generalizations adjacent to v.
			OGDF_ASSERT(genNodes.size() <= 2);

			// Now introduce the circular list of new edges
			// forming the border of the merge face. Keep the embedding.
			adjEntry adjPrev = v->firstAdj();

			for (itn = expander.begin(); itn.valid(); ++itn) {
				e = newEdge(adjPrev, (*itn)->firstAdj());
				if (!expandAdj(v)) {
					expandAdj(v) = e->adjSource();
				}
				typeOf(e) = EdgeType::association; //???
				setExpansionEdge(e, 2);

				//new types
				setAssociation(e); //should be dummy type?
				setExpansion(e);

				adjPrev = (*itn)->firstAdj();
			}
			e = newEdge(adjPrev, v->lastAdj());
			typeOf(e) = EdgeType::association; //???
			setExpansionEdge(e, 2);


			if (genNodes.size() == 2) {
				node u = genNodes.popFrontRet();
				node w = genNodes.popFrontRet();
				e = newEdge(u->firstAdj()->succ(), w->firstAdj()->succ());
				m_faceSplitter[e] = true;
			}

			OGDF_ASSERT(representsCombEmbedding());
		}
	}
}

void PlanRepUML::expandLowDegreeVertices(OrthoRep& OR, bool alignSmallDegree) {
	for (node v : nodes) {
		if (!(isVertex(v)) || expandAdj(v) != nullptr) {
			continue;
		}

		int startDegree = v->degree();

		SList<edge> adjEdges;
		SListPure<Tuple2<node, int>> expander;

		node u = v;
		bool firstTime = true;

		setExpandedNode(v, v); //obsolete?! u=v

		for (adjEntry adj : v->adjEntries) {
			adjEdges.pushBack(adj->theEdge());

			if (!firstTime) {
				u = newNode();
			}

			setExpandedNode(u, v);
			typeOf(u) = Graph::NodeType::lowDegreeExpander;
			expander.pushBack(Tuple2<node, int>(u, OR.angle(adj)));
			firstTime = false;
		}


		SListConstIterator<edge> it;
		SListConstIterator<Tuple2<node, int>> itn;

		itn = expander.begin().succ();

		for (it = adjEdges.begin().succ(); it.valid(); ++it) {
			// Did we allocate enough dummy nodes?
			OGDF_ASSERT(itn.valid());

			if ((*it)->source() == v) {
				moveSource(*it, (*itn).x1());
			} else {
				moveTarget(*it, (*itn).x1());
			}
			++itn;
		}

		adjEntry adjPrev = v->firstAdj();
		itn = expander.begin();
		int nBends = (*itn).x2();

		edge e;
		for (++itn; itn.valid(); ++itn) {
			e = newEdge(adjPrev, (*itn).x1()->firstAdj());

			OR.bend(e->adjSource()).set(OrthoBendType::convexBend, nBends);
			OR.bend(e->adjTarget()).set(OrthoBendType::reflexBend, nBends);
			OR.angle(adjPrev) = 1;
			OR.angle(e->adjSource()) = 2;
			OR.angle(e->adjTarget()) = 1;

			nBends = (*itn).x2();

			typeOf(e) = EdgeType::association; //???
			setExpansionEdge(e, 2);

			adjPrev = (*itn).x1()->firstAdj();
		}

		e = newEdge(adjPrev, v->lastAdj());
		typeOf(e) = EdgeType::association; //???
		setExpansionEdge(e, 2);

		expandAdj(v) = e->adjSource();

		OR.bend(e->adjSource()).set(OrthoBendType::convexBend, nBends);
		OR.bend(e->adjTarget()).set(OrthoBendType::reflexBend, nBends);
		OR.angle(adjPrev) = 1;
		OR.angle(e->adjSource()) = 2;
		OR.angle(e->adjTarget()) = 1;

		if (alignSmallDegree && (startDegree == 2)) {
			node vOpp = e->source();
			if (vOpp == v) {
				vOpp = e->target(); //only one case necessary
			}
			edge eAlign = newEdge(vOpp->lastAdj(), vOpp->lastAdj()->faceCycleSucc());
			typeOf(eAlign) = EdgeType::association;
			OR.angle(eAlign->adjSource()) = 1;
			OR.angle(eAlign->adjTarget()) = 1;
			OR.angle(eAlign->adjSource()->faceCycleSucc()) = 1;
			OR.angle(eAlign->adjTarget()->faceCycleSucc()) = 1;
		}
	}
}

void PlanRepUML::collapseVertices(const OrthoRep& OR, Layout& drawing) {
	for (node v : nodes) {
		const OrthoRep::VertexInfoUML* vi = OR.cageInfo(v);

		if (vi == nullptr
				|| (typeOf(v) != Graph::NodeType::highDegreeExpander
						&& typeOf(v) != Graph::NodeType::lowDegreeExpander)) {
			continue;
		}

		node vOrig = original(v);
		OGDF_ASSERT(vOrig != nullptr);

		node vCenter = newNode();
		m_vOrig[vCenter] = vOrig;
		m_vCopy[vOrig] = vCenter;
		m_vOrig[v] = nullptr;

		node lowerLeft = vi->m_corner[static_cast<int>(OrthoDir::North)]->theNode();
		node lowerRight = vi->m_corner[static_cast<int>(OrthoDir::West)]->theNode();
		node upperLeft = vi->m_corner[static_cast<int>(OrthoDir::East)]->theNode();
		drawing.x(vCenter) = 0.5 * (drawing.x(lowerLeft) + drawing.x(lowerRight));
		drawing.y(vCenter) = 0.5 * (drawing.y(lowerLeft) + drawing.y(upperLeft));

		//Attention: The order in which the connection nodes are inserted
		//ist not in the order of the copy embedding, but in the order
		//of the adjacency lists of the original graph. Therefore, the edges
		//may not be used to derive the correct copy order of outgoing edges.
		//We compute a list of edges corresponding to the embedding and use
		//this list to insert the edges. The order is e.g. used for clique positioning
		List<edge> adjEdges;
		//we start at an arbitrary corner
		adjEntry adjCorner = vi->m_corner[static_cast<int>(OrthoDir::North)];
		do {
			adjEntry runAdj = adjCorner->twin();
			edge eOrig = nullptr;
#ifdef OGDF_DEBUG
			int count = 0; //should be max. 4 (3) edges at boundary node
#endif
			//the order of the edges in the copy may be incorrect, we search for the
			//edge with an original
			// do {
			runAdj = runAdj->cyclicSucc();
			eOrig = original(runAdj->theEdge());
#ifdef OGDF_DEBUG
			count++;
#endif
			// } while ((count < 4) && !eOrig);
			//edge found or corner reached
			OGDF_ASSERT((count == 1) || (runAdj->theNode()->degree() == 2));
			if (eOrig) {
				adjEdges.pushBack(eOrig);
			}
			adjCorner = adjCorner->faceCycleSucc(); //TODO: pred?
		} while (adjCorner != vi->m_corner[static_cast<int>(OrthoDir::North)]);

		OGDF_ASSERT(adjEdges.size() == vOrig->degree());
		ListIterator<edge> itEdge = adjEdges.begin();

#if 0
		edge eOrig;
		forall_adj_edges(eOrig,vOrig) {
#else
		while (itEdge.valid()) {
			edge eOrig = *itEdge;
#endif
		if (eOrig->target() == vOrig) {
			node connect = m_eCopy[eOrig].back()->target();
			edge eNew = newEdge(connect, vCenter);
			m_eOrig[eNew] = eOrig;
			m_eIterator[eNew] = m_eCopy[eOrig].pushBack(eNew);
		} else {
			node connect = m_eCopy[eOrig].front()->source();
			edge eNew = newEdge(vCenter, connect);
			m_eOrig[eNew] = eOrig;
			m_eIterator[eNew] = m_eCopy[eOrig].pushFront(eNew);
		}
		++itEdge;
	}
}
}

void PlanRepUML::setupIncremental(int indexCC, CombinatorialEmbedding& E) {
	prepareIncrementalMergers(indexCC, E);
}

void PlanRepUML::prepareIncrementalMergers(int indexCC, CombinatorialEmbedding& E) {
	//we can't draw multiple hierarchies hanging at one class
	//object, therefore we reduce the number in the given layout
	//to 1 by interpreting only the edges in the largest sequence
	//of generalizations as gens, all other edges are associations
	//const Graph& G = uml;

	for (node v : nodes) {
		if (v->degree() < 2) {
			continue;
		}
		if (typeOf(v) == Graph::NodeType::generalizationMerger) {
			continue;
		}

		int maxSeq = 0; //stores current best sequence size
		int maxSeqRun = 0; //stores current sequence size

		adjEntry maxSeqAdj = nullptr;
		adjEntry runSeqAdj = nullptr;
		adjEntry ad1 = v->firstAdj();
		//We have to avoid the case where we start within a sequence
		//we run back til the first non-input generalization is detected
		//if there is none, the following is also correct
		adjEntry stopAdj = ad1;
		while (ad1->cyclicPred() != stopAdj && ad1->theEdge()->target() == v
				&& isGeneralization(ad1->theEdge())) {
			ad1 = ad1->cyclicPred();
		}
		adjEntry ad = ad1->cyclicSucc();
		while (ad != ad1) {
			edge e = ad->theEdge();
			if (e->target() == v && isGeneralization(e)) {
				if (maxSeqRun == 0) {
					//initialize once
					runSeqAdj = ad;
					maxSeqAdj = ad;
				}
				maxSeqRun++;

			} else {
				//we never stop here if there is only one
				//gen sequence, but then we don't need to
				//(we don't use maxSeq elsewhere)
				//we change edge in both cases here to avoid
				//running over all edges again (#gens may be
				//significantly lower then #ass)
				adjEntry changeAdj = nullptr;
				if (maxSeqRun > maxSeq) {
					//change edge type for old favorites
					if (maxSeqAdj != runSeqAdj) {
						changeAdj = maxSeqAdj;
					}

					maxSeq = maxSeqRun;
					maxSeqAdj = runSeqAdj;
				} else {
					//change edge types for weaker sequence
					if (maxSeqRun != 0) {
						changeAdj = runSeqAdj;
					}
				}

				//Change types for a sequence
				//invariant: on every pass of the loop, if a sequence
				//end is detected, one of the two sequences is deleted
				//(types changed) => only one sequence when we stop
				if (changeAdj != nullptr) {
					adjEntry runGenAdj = changeAdj;
					//no infinite loop because new sequence found
					edge eRGA = runGenAdj->theEdge();
					while ((eRGA->target() == v) && isGeneralization(eRGA)) {
						setAssociation(eRGA);
						runGenAdj = runGenAdj->cyclicSucc();
						eRGA = runGenAdj->theEdge();
					}
#if 0
					changeAdj = 0;
#endif
				}
				maxSeqRun = 0;
			}

			ad = ad->cyclicSucc();
		}

		//now we insert mergers for all edges in best sequence
		//do not use maxSeq to count, may be 0 if only incoming gens

		if (maxSeqAdj != nullptr) {
			SList<edge> inGens;

			edge eMSA = maxSeqAdj->theEdge();
			adjEntry runAdj = maxSeqAdj;
			while ((eMSA->target() == v) && isGeneralization(eMSA)) {
				inGens.pushBack(eMSA);
				runAdj = runAdj->cyclicSucc();
				eMSA = runAdj->theEdge();
				//maybe only one sequence around v
				if (runAdj == maxSeqAdj) {
					break;
				}
			}

			//insert the merger for v
			OGDF_ASSERT(representsCombEmbedding());
			node newMerger = insertGenMerger(v, inGens, E);
			OGDF_ASSERT(representsCombEmbedding());
			if (newMerger) {
				m_incMergers[indexCC].pushBack(newMerger);
			}
		}
	}

#if 0
	uml.adjustHierarchyParents();
#endif
}

//inserts a merger node for generalizations hanging at v, respecting
//embedding E
node PlanRepUML::insertGenMerger(node /* v */, const SList<edge>& inGens, CombinatorialEmbedding& E) {
	node u = nullptr;
	if (empty()) {
		return u;
	}
	if (inGens.size() >= 2) {
		// create a new node representing the merge point for the generalizations
		u = newNode();
		typeOf(u) = Graph::NodeType::generalizationMerger;

		//store the embedding information before inserting objects
		//TODO: Front or back
		face fRight = E.rightFace(inGens.front()->adjSource());
		face fLeft = E.rightFace(inGens.back()->adjTarget());
		// add the edge from v to the merge point
		// this edge is a generalization, but has no original edge
		//edge eMerge = insertEdge(u, (*(inGens.rbegin()))->adjTarget(), E);
		edge eMerge = newEdge(u, inGens.back()->adjTarget());
		//newEdge(u, (*(inGens.rbegin()))->adjTarget()); //incoming generalization
		typeOf(eMerge) = Graph::EdgeType::generalization;
		m_mergeEdges.pushBack(eMerge);

		// We move the target node of each ingoing generalization of v to u.
		// Note that, for each such edge e, the target node of the original
		// edge is then different from the original of the target node of e
		// (the latter is 0 because u is a new (dummy) node)
		SListConstIterator<edge> it;
		for (it = inGens.begin(); it.valid(); ++it) {
			// all edges in the list inGens must be ingoing generalizations of v
#if 0
			OGDF_ASSERT((*it)->target() == v);
			OGDF_ASSERT(typeOf(*it) == Graph::generalization);
#endif

			moveTarget(*it, u);
		}
		//now we update the combinatorial embedding to represent the new situation
		//first, update the face information at the inserted edge
		E.updateMerger(eMerge, fRight, fLeft);
	}

	return u;
}

// Same as in GraphAttributes. Except: Writes colors to new nodes and
// to generalizations. For debugging only
void PlanRepUML::writeGML(const char* fileName, const Layout& drawing) {
	std::ofstream os(fileName);
	writeGML(os, drawing);
}

void PlanRepUML::writeGML(const char* fileName) {
	Layout drawing(*this);
	std::ofstream os(fileName);
	writeGML(os, drawing);
}

//zu debugzwecken
void PlanRepUML::writeGML(const char* fileName, GraphAttributes& AG) {
	OGDF_ASSERT(m_pGraphAttributes == &(AG));
	Layout drawing(*this);
	for (node v : nodes) {
		if (original(v)) {
			drawing.x(v) = AG.x(original(v));
			drawing.y(v) = AG.y(original(v));
		}
	}

	std::ofstream os(fileName);
	writeGML(os, drawing);
}

void PlanRepUML::writeGML(std::ostream& os, const Layout& drawing) {
	const Graph& G = *this;

	NodeArray<int> id(*this);
	int nextId = 0;

	os.setf(std::ios::showpoint);
	os.precision(10);

	os << "Creator \"ogdf::GraphAttributes::writeGML\"\n";
	os << "graph [\n";
	os << "  directed 1\n";

	for (node v : G.nodes) {
		os << "  node [\n";

		os << "    id " << (id[v] = nextId++) << "\n";
#ifdef OGDF_DEBUG
		os << "    label \"" << v->index() << "\"\n";
#endif

		os << "    graphics [\n";
		os << "      x " << drawing.x(v) << "\n";
		os << "      y " << drawing.y(v) << "\n";
		os << "      w " << 10.0 << "\n";
		os << "      h " << 10.0 << "\n";
		os << "      type \"rectangle\"\n";
		os << "      width 1.0\n";
		if (typeOf(v) == Graph::NodeType::generalizationMerger) {
			os << "      type \"oval\"\n";
			os << "      fill \"#0000A0\"\n";
		} else if (typeOf(v) == Graph::NodeType::generalizationExpander) {
			os << "      type \"oval\"\n";
			os << "      fill \"#00FF00\"\n";
		} else if (typeOf(v) == Graph::NodeType::highDegreeExpander
				|| typeOf(v) == Graph::NodeType::lowDegreeExpander) {
			os << "      fill \"#FFFF00\"\n";
		} else if (typeOf(v) == Graph::NodeType::dummy) {
			if (isCrossingType(v)) {
				os << "      fill \"#FF0000\"\n";
			} else {
				os << "      fill \"#FFFFFF\"\n";
			}
			os << "      type \"oval\"\n";
		}

		else if (v->degree() > 4) {
			os << "      fill \"#FFFF00\"\n";
		}

		else {
			os << "      fill \"#000000\"\n";
		}


		os << "    ]\n"; // graphics

		os << "  ]\n"; // node
	}


	for (edge e : G.edges) {
		os << "  edge [\n";

		os << "    source " << id[e->source()] << "\n";
		os << "    target " << id[e->target()] << "\n";

		os << "    generalization " << typeOf(e) << "\n";

		os << "    graphics [\n";

		os << "      type \"line\"\n";

		if (typeOf(e) == Graph::EdgeType::generalization) {
			os << "      arrow \"last\"\n";
			if (m_alignUpward[e->adjSource()]) {
				os << "      fill \"#0000FF\"\n";
			} else {
				os << "      fill \"#FF0000\"\n";
			}
			os << "      width 3.0\n";
		} else {
			if (typeOf(e->source()) == Graph::NodeType::generalizationExpander
					|| typeOf(e->source()) == Graph::NodeType::generalizationMerger
					|| typeOf(e->target()) == Graph::NodeType::generalizationExpander
					|| typeOf(e->target()) == Graph::NodeType::generalizationMerger) {
				os << "      arrow \"none\"\n";
				if (isBrother(e)) {
					os << "      fill \"#F0F000\"\n"; //gelb
				} else if (isHalfBrother(e)) {
					os << "      fill \"#FF00AF\"\n";
				} else {
					os << "      fill \"#FF0000\"\n";
				}
			} else {
				os << "      arrow \"none\"\n";
			}
			if (isBrother(e)) {
				os << "      fill \"#F0F000\"\n"; //gelb
			} else if (isHalfBrother(e)) {
				os << "      fill \"#FF00AF\"\n";
			} else if (!(original(e))) {
				os << "      fill \"#00F00F\"\n";
			} else {
				os << "      fill \"#00000F\"\n";
			}
			os << "      width 1.0\n";
		}
		os << "    ]\n"; // graphics

		os << "  ]\n"; // edge
	}

	os << "]\n"; // graph
}

void PlanRepUML::writeGML(const char* fileName, const OrthoRep& OR, const Layout& drawing) {
	std::ofstream os(fileName);
	writeGML(os, OR, drawing);
}

void PlanRepUML::writeGML(std::ostream& os, const OrthoRep& OR, const Layout& drawing) {
	const Graph& G = *this;

	NodeArray<int> id(*this);
	int nextId = 0;

	os.setf(std::ios::showpoint);
	os.precision(10);

	os << "Creator \"ogdf::GraphAttributes::writeGML\"\n";
	os << "graph [\n";
	os << "  directed 1\n";

	for (node v : G.nodes) {
		os << "  node [\n";

		os << "    id " << (id[v] = nextId++) << "\n";

		os << "    label \"" << v->index() << "\"\n";

		os << "    graphics [\n";
		os << "      x " << drawing.x(v) << "\n";
		os << "      y " << drawing.y(v) << "\n";
		os << "      w " << 3.0 << "\n";
		os << "      h " << 3.0 << "\n";
		os << "      type \"rectangle\"\n";
		os << "      width 1.0\n";
		if (typeOf(v) == Graph::NodeType::generalizationMerger) {
			os << "      type \"oval\"\n";
			os << "      fill \"#0000A0\"\n";
		} else if (typeOf(v) == Graph::NodeType::generalizationExpander) {
			os << "      type \"oval\"\n";
			os << "      fill \"#00FF00\"\n";
		} else if (typeOf(v) == Graph::NodeType::highDegreeExpander
				|| typeOf(v) == Graph::NodeType::lowDegreeExpander) {
			os << "      fill \"#FFFF00\"\n";
		} else if (typeOf(v) == Graph::NodeType::dummy) {
			os << "      type \"oval\"\n";
		}

		else if (v->degree() > 4) {
			os << "      fill \"#FFFF00\"\n";
		}

		else {
			os << "      fill \"#000000\"\n";
		}


		os << "    ]\n"; // graphics

		os << "  ]\n"; // node
	}

	for (node v : nodes) {
		if (expandAdj(v) != nullptr
				&& (typeOf(v) == Graph::NodeType::highDegreeExpander
						|| typeOf(v) == Graph::NodeType::lowDegreeExpander)) {
			node vOrig = original(v);
			const OrthoRep::VertexInfoUML& vi = *OR.cageInfo(v);
			node ll = vi.m_corner[static_cast<int>(OrthoDir::North)]->theNode();
			node ur = vi.m_corner[static_cast<int>(OrthoDir::South)]->theNode();

			os << "  node [\n";
			os << "    id " << nextId++ << "\n";

			if (m_pGraphAttributes->has(GraphAttributes::nodeLabel)) {
				os << "    label \"" << m_pGraphAttributes->label(vOrig) << "\"\n";
			}

			os << "    graphics [\n";
			os << "      x " << 0.5 * (drawing.x(ur) + drawing.x(ll)) << "\n";
			os << "      y " << 0.5 * (drawing.y(ur) + drawing.y(ll)) << "\n";
			os << "      w " << widthOrig(vOrig) << "\n";
			os << "      h " << heightOrig(vOrig) << "\n";
			os << "      type \"rectangle\"\n";
			os << "      width 1.0\n";
			os << "      fill \"#FFFF00\"\n";

			os << "    ]\n"; // graphics
			os << "  ]\n"; // node
		}
	}

	for (edge e : G.edges) {
		os << "  edge [\n";

		os << "    source " << id[e->source()] << "\n";
		os << "    target " << id[e->target()] << "\n";

		os << "    generalization " << typeOf(e) << "\n";

		os << "    graphics [\n";

		os << "      type \"line\"\n";

		if (typeOf(e) == Graph::EdgeType::generalization) {
			if (typeOf(e->target()) == Graph::NodeType::generalizationExpander) {
				os << "      arrow \"none\"\n";
			} else {
				os << "      arrow \"last\"\n";
			}

			os << "      fill \"#FF0000\"\n";
			os << "      width 2.0\n";
		} else {
			if (typeOf(e->source()) == Graph::NodeType::generalizationExpander
					|| typeOf(e->source()) == Graph::NodeType::generalizationMerger
					|| typeOf(e->target()) == Graph::NodeType::generalizationExpander
					|| typeOf(e->target()) == Graph::NodeType::generalizationMerger) {
				os << "      arrow \"none\"\n";
				os << "      fill \"#FF0000\"\n";
			} else if (original(e) == nullptr) {
				os << "      arrow \"none\"\n";
				os << "      fill \"#AFAFAF\"\n";
			} else {
				os << "      arrow \"none\"\n";
			}
			if (isBrother(e)) {
				os << "      fill \"#00AF0F\"\n";
			}
			if (isHalfBrother(e)) {
				os << "      fill \"#0F00AF\"\n";
			}
			os << "      width 1.0\n";
		}

		os << "    ]\n"; // graphics

		os << "  ]\n"; // edge
	}

	os << "]\n"; // graph
}

void PlanRepUML::writeGML(const char* fileName, const OrthoRep& OR, const GridLayoutMapped& drawing) {
	std::ofstream os(fileName);
	writeGML(os, OR, drawing);
}

void PlanRepUML::writeGML(std::ostream& os, const OrthoRep& OR, const GridLayoutMapped& drawing) {
	const Graph& G = *this;

	NodeArray<int> id(*this);
	int nextId = 0;

	os.setf(std::ios::showpoint);
	os.precision(10);

	os << "Creator \"ogdf::GraphAttributes::writeGML\"\n";
	os << "graph [\n";
	os << "  directed 1\n";

	for (node v : G.nodes) {
		os << "  node [\n";

		os << "    id " << (id[v] = nextId++) << "\n";

		os << "    label \"" << v->index() << "\"\n";

		os << "    graphics [\n";
		os << "      x " << drawing.toDouble(drawing.x(v)) << "\n";
		os << "      y " << drawing.toDouble(drawing.y(v)) << "\n";
		os << "      w " << 3.0 << "\n";
		os << "      h " << 3.0 << "\n";
		os << "      type \"rectangle\"\n";
		os << "      width 1.0\n";
		if (typeOf(v) == Graph::NodeType::generalizationMerger) {
			os << "      type \"oval\"\n";
			os << "      fill \"#0000A0\"\n";
		} else if (typeOf(v) == Graph::NodeType::generalizationExpander) {
			os << "      type \"oval\"\n";
			os << "      fill \"#00FF00\"\n";
		} else if (typeOf(v) == Graph::NodeType::highDegreeExpander
				|| typeOf(v) == Graph::NodeType::lowDegreeExpander) {
			os << "      fill \"#FFFF00\"\n";
		} else if (typeOf(v) == Graph::NodeType::dummy) {
			os << "      type \"oval\"\n";
		}

		else if (v->degree() > 4) {
			os << "      fill \"#FFFF00\"\n";
		}

		else {
			os << "      fill \"#000000\"\n";
		}


		os << "    ]\n"; // graphics

		os << "  ]\n"; // node
	}

	for (node v : nodes) {
		if (expandAdj(v) != nullptr
				&& (typeOf(v) == Graph::NodeType::highDegreeExpander
						|| typeOf(v) == Graph::NodeType::lowDegreeExpander)) {
			node vOrig = original(v);
			const OrthoRep::VertexInfoUML& vi = *OR.cageInfo(v);
			node ll = vi.m_corner[static_cast<int>(OrthoDir::North)]->theNode();
			node ur = vi.m_corner[static_cast<int>(OrthoDir::South)]->theNode();

			os << "  node [\n";
			os << "    id " << nextId++ << "\n";

			if (m_pGraphAttributes->has(GraphAttributes::nodeLabel)) {
				os << "    label \"" << m_pGraphAttributes->label(vOrig) << "\"\n";
			} else {
				os << "    label \"N " << vOrig->index() << "\"\n";
			}

			os << "    graphics [\n";
			os << "      x " << 0.5 * drawing.toDouble(drawing.x(ur) + drawing.x(ll)) << "\n";
			os << "      y " << 0.5 * drawing.toDouble(drawing.y(ur) + drawing.y(ll)) << "\n";
			os << "      w " << widthOrig(vOrig) << "\n";
			os << "      h " << heightOrig(vOrig) << "\n";
			os << "      type \"rectangle\"\n";
			os << "      width 1.0\n";
			os << "      fill \"#FFFF00\"\n";

			os << "    ]\n"; // graphics
			os << "  ]\n"; // node
		}
	}

	for (edge e : G.edges) {
		os << "  edge [\n";

		os << "    source " << id[e->source()] << "\n";
		os << "    target " << id[e->target()] << "\n";

		os << "    generalization " << typeOf(e) << "\n";

		os << "    graphics [\n";

		os << "      type \"line\"\n";

		if (typeOf(e) == Graph::EdgeType::generalization) {
			if (typeOf(e->target()) == Graph::NodeType::generalizationExpander) {
				os << "      arrow \"none\"\n";
			} else {
				os << "      arrow \"last\"\n";
			}

			//check the vertical compaction mode
			if ((typeOf(e) == Graph::EdgeType::generalization) && //only generalizations
					!isExpansionEdge(e)) {
				if ((e->adjSource() == OR.externalAdjEntry())
						|| (e->adjTarget() == OR.externalAdjEntry())) {
					os << "      fill \"#00FF00\"\n";
				} else if ((e->adjSource() == OR.alignAdjEntry())
						|| (e->adjTarget() == OR.alignAdjEntry())) {
					os << "      fill \"#FFA000\"\n";
				} else {
					os << "      fill \"#0000FF\"\n";
				}
			} else if ((e->adjSource() == OR.externalAdjEntry())
					|| (e->adjTarget() == OR.externalAdjEntry())) {
				os << "      fill \"#00FF00\"\n";
			} else if ((e->adjSource() == OR.alignAdjEntry())
					|| (e->adjTarget() == OR.alignAdjEntry())) {
				os << "      fill \"#FFA000\"\n";
			} else {
				os << "      fill \"#FF0000\"\n";
			}
			os << "      width 2.0\n";
		} else {
			if (typeOf(e->source()) == Graph::NodeType::generalizationExpander
					|| typeOf(e->source()) == Graph::NodeType::generalizationMerger
					|| typeOf(e->target()) == Graph::NodeType::generalizationExpander
					|| typeOf(e->target()) == Graph::NodeType::generalizationMerger) {
				os << "      arrow \"none\"\n";

				if (((e->adjSource() == OR.externalAdjEntry())
							|| (e->adjTarget() == OR.externalAdjEntry()))
						|| ((e->adjSource() == OR.alignAdjEntry())
								|| (e->adjTarget() == OR.alignAdjEntry()))) {
					os << "      fill \"#00FF00\"\n";
				} else {
					os << "      fill \"#F0F00F\"\n";
				}
				//os << "      fill \"#FF0000\"\n";
			} else if (original(e) == nullptr) {
				os << "      arrow \"none\"\n";
				if (((e->adjSource() == OR.externalAdjEntry())
							|| (e->adjTarget() == OR.externalAdjEntry()))
						|| ((e->adjSource() == OR.alignAdjEntry())
								|| (e->adjTarget() == OR.alignAdjEntry()))) {
					os << "      fill \"#00FF00\"\n";
				} else {
					os << "      fill \"#AFAFAF\"\n";
				}
			} else {
				os << "      arrow \"none\"\n";
			}

			os << "      width 1.0\n";
		}

		os << "    ]\n"; // graphics

		os << "  ]\n"; // edge
	}

	os << "]\n"; // graph
}
}
