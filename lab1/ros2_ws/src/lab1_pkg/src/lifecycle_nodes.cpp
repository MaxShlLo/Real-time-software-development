#include <memory>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "lifecycle_msgs/msg/transition.hpp"
#include "rclcpp_lifecycle/lifecycle_node.hpp"

using LNI = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface;

// Клас керованого вузла
class MyLifecycleNode : public rclcpp_lifecycle::LifecycleNode
{
public:
  explicit MyLifecycleNode(const std::string & node_name)
  : rclcpp_lifecycle::LifecycleNode(node_name)
  {
      RCLCPP_INFO(get_logger(), "Вузол '%s' створено (стан: Unconfigured)", node_name.c_str());
  }

  // Перевизначення методів переходів станів (Transitions)
  LNI::CallbackReturn on_configure(const rclcpp_lifecycle::State &)
  {
    RCLCPP_INFO(get_logger(), "on_configure() викликано. Перехід до Inactive.");
    return LNI::CallbackReturn::SUCCESS;
  }

  LNI::CallbackReturn on_activate(const rclcpp_lifecycle::State &)
  {
    RCLCPP_INFO(get_logger(), "on_activate() викликано. Перехід до Active.");
    return LNI::CallbackReturn::SUCCESS;
  }

  LNI::CallbackReturn on_deactivate(const rclcpp_lifecycle::State &)
  {
    RCLCPP_INFO(get_logger(), "on_deactivate() викликано. Перехід до Inactive.");
    return LNI::CallbackReturn::SUCCESS;
  }

  LNI::CallbackReturn on_cleanup(const rclcpp_lifecycle::State &)
  {
    RCLCPP_INFO(get_logger(), "on_cleanup() викликано. Перехід до Unconfigured.");
    return LNI::CallbackReturn::SUCCESS;
  }
};

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  // 1. Створюємо два керовані вузли, як вимагається у завданні
  auto node1 = std::make_shared<MyLifecycleNode>("lc_node_1");
  auto node2 = std::make_shared<MyLifecycleNode>("lc_node_2");

  // Для простоти демонстрації у цій лабораторній ми змусимо вузли 
  // самостійно викликати переходи, щоб ви побачили зміну станів у логах.
  // На практиці це робить Lifecycle Manager.

 // 2. Конфігуруємо (перехід Unconfigured -> Inactive)
  node1->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
  node2->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);

  // 3. Активуємо (перехід Inactive -> Active)
  node1->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
  node2->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);

  // Спіннер, щоб вузли залишалися активними і ви могли їх побачити
  rclcpp::executors::SingleThreadedExecutor executor;
  executor.add_node(node1->get_node_base_interface());
  executor.add_node(node2->get_node_base_interface());
  executor.spin();

  rclcpp::shutdown();
  return 0;
}