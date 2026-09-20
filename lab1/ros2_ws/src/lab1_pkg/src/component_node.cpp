#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"

namespace lab1_namespace
{
    class ComponentNode : public rclcpp::Node
    {
    public:
        explicit ComponentNode(const rclcpp::NodeOptions & options)
        : Node("my_component_node", options)
        {
            RCLCPP_INFO(this->get_logger(), "The component node has been successfully initialized");
        }
    };
}

RCLCPP_COMPONENTS_REGISTER_NODE(lab1_namespace::ComponentNode)