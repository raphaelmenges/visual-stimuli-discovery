#include "DecisionTree.hpp"

#include <Core/Core.hpp>
#include <deque>

namespace learn
{
	DecisionTree::DecisionTree(std::shared_ptr<const data::Dataset> sp_dataset)
		:
		MulticlassClassifier<shogun::CCARTree>(sp_dataset)
	{
		// Create classifier
		_sp_classifier = std::make_shared<shogun::Some<shogun::CCARTree> >(shogun::some<shogun::CCARTree>(*_sp_feature_types, shogun::EProblemType::PT_MULTICLASS));
		(*_sp_classifier)->set_labels(*_sp_labels);
		(*_sp_classifier)->train(*_sp_features);
	}

	// Destructor
	DecisionTree::~DecisionTree()
	{
		// Nothing to do
	}

	void DecisionTree::print() const
	{
		core::mt::log_info("Printing decision tree classifier...");

		// Prepare datastructure
		typedef shogun::CTreeMachineNode<shogun::CARTreeNodeData> NodeType;
		std::deque<std::pair<NodeType*, int> > nodes;

		// Push back root node
		auto p_root = (*_sp_classifier)->get_root(); 
		nodes.push_back({ p_root, 0 });
		while (!nodes.empty())
		{
			// Pop front element from queue
			auto& r_pair = nodes.front();
			auto p_node = r_pair.first;
			int depth = r_pair.second;
			nodes.pop_front();

			// Get chilren
			shogun::CDynamicObjectArray* p_children = p_node->get_children();

			// Preceeding symbols
			std::string space = "|";
			for (int i = 0; i < depth; ++i)
			{
				space = space + "|";
			}

			// Get transit into values
			// Note: transit value of children is used to compare value of feature.
			// If feature value is smaller or equal transit size, left child is followed. Otherwise right child is followed
			// left_child->data.transit_if_feature_value>=sample[node->data.attribute_id]
			auto transit_vector = p_node->data.transit_into_values;
			std::string transits;
			for (int i = 0; i < transit_vector.size(); ++i)
			{
				// Remark: If normalized features have been used, these thresholds are also between zero and one
				transits += std::to_string(transit_vector[i]) + "; ";
			}

			// Decide type of node
			if (p_children->get_num_elements() > 0)
			{
				// It is a split node
				// Note: for binary tree, child at index 0 is left, at index 1 is right
				core::mt::log_info(space + "Split: ", _feature_names.at(p_node->data.attribute_id), ", Label: ", p_node->data.node_label, ", Transits: ", transits);
			}
			else
			{
				// It is a leaf node
				core::mt::log_info(space + "Leaf: ", p_node->data.node_label, ", Transits: ", transits);
			}

			// Add children to queue
			for (int i = 0; i < p_children->get_num_elements(); ++i)
			{
				NodeType* current = dynamic_cast<NodeType*>(p_children->get_element(i));
				nodes.push_front({ current, depth + 1 });
			}

			// Unreference both children and node
			SG_UNREF(p_children);
			SG_UNREF(p_node);
		}
	}
}