#include "rclcpp/rclcpp.hpp"
#include "rclcpp_components/register_node_macro.hpp"

// Створюємо власний namespace
namespace lab1_namespace
{
    // Клас, успадкований від rclcpp::Node
    class ComponentNode : public rclcpp::Node
    {
    public:
        // Конструктор приймає rclcpp::NodeOptions, як вимагається у завданні
        explicit ComponentNode(const rclcpp::NodeOptions & options)
        : Node("my_component_node", options)
        {
            // Сам конструктор поки лишаємо порожнім, додаємо лише лог для перевірки
            RCLCPP_INFO(this->get_logger(), "The component node has been successfully initialized!");
        }
    };
} // namespace lab1_namespace

// Реєстрація вузла як компонента
RCLCPP_COMPONENTS_REGISTER_NODE(lab1_namespace::ComponentNode)