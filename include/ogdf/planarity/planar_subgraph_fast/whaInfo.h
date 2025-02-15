/** \file
 * \brief Declaration of class whaInfo.
 *
 * \author Sebastian Leipert
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

#include <ogdf/basic/memory.h>

namespace ogdf {
class PQNodeRoot;

/**
 * The definitions for W, B, H and A
 * describe the type of a node during the computation of the
 * maximal pertinent sequence. A pertinent node X in the PQ-tree will be
 * either of type B, W, A or H. Together
 * with some other information stored at every node the pertinent leaves
 * in the frontier of X that have to be deleted. For further
 * description of the types see Jayakumar, Thulasiraman and Swamy 1989.
 */

enum class whaType { W, B, H, A };

class whaInfo {
	template<class T, class Y>
	friend class MaxSequencePQTree;

public:
	//The deleteType is set to type b (= keep all leaves in the frontier).
	whaInfo() {
		m_a = 0;
		m_h = 0;
		m_w = 0;
		m_deleteType = whaType::B;
		m_pertLeafCount = 0;
		m_notVisitedCount = 0;
		m_aChild = nullptr;
		m_hChild1 = nullptr;
		m_hChild2 = nullptr;
		m_hChild2Sib = nullptr;
	}

	~whaInfo() { }

	void defaultValues() {
		m_a = 0;
		m_h = 0;
		m_w = 0;
		m_deleteType = whaType::B;
		m_pertLeafCount = 0;
		m_notVisitedCount = 0;
	}

private:
	// number of pertinent leaves in the frontier of the node respectively the
	// number of leaves that have to be deleted in the frontier of the node to
	// make it an empty node.
	int m_h;

	// number of pertinent leaves in the frontier of the node that have to be
	// deleted in order to create a node of type h, that is a node, where a
	// permutation of the leaves of the node exist such that the remaining
	// pertinent leaves form a consecutive sequence on one end of the permutation.
	int m_w;

	// number of pertinent leaves in the frontier of the node that have to be
	// deleted in order to create a node of type $a$, that is a node, where a
	// permutation of the leaves of the node exist such that the remaining
	// pertinent leaves form a consecutive somewhere within the permutation.
	int m_a;

	// deleteType is type of the node being either
	// W, B, H or A.
	whaType m_deleteType;

	//the number of pertinent leaves in the frontier of a node.
	int m_pertLeafCount;

	//counts the number of pertinent children, that have not been
	// processed yet during the computation of the w,h,a-numbering.
	int m_notVisitedCount;

	// a pointer to the child of node that has to be of type a if the
	// node itself has been determined to be of type a.
	PQNodeRoot* m_aChild;

	// a pointer to the child of node that has to be of type h if the
	// node itself has been determined to be of type h.
	PQNodeRoot* m_hChild1;

	// a pointer to the child of node that has to be of type h if the
	// node itself has been determined to be of type a and m_aChild does
	// contain the empty pointer.
	PQNodeRoot* m_hChild2;


	// m_hChild2Sib is a pointer to the pertinent sibling of m_hChild2. This
	// pointer is necessary if the sequence of pertinent children is not unique.
	PQNodeRoot* m_hChild2Sib;

	OGDF_NEW_DELETE
};

}
