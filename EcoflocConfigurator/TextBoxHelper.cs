using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows;

namespace EcoflocConfigurator
{
    public static class TextBoxHelper
    {
        public static readonly DependencyProperty HintProperty = DependencyProperty.RegisterAttached(
            "Hint", typeof(string), typeof(TextBoxHelper), new PropertyMetadata(default(string), OnHintChanged));

        public static void SetHint(UIElement element, string value) => element.SetValue(HintProperty, value);
        public static string GetHint(UIElement element) => (string)element.GetValue(HintProperty);

        private static void OnHintChanged(DependencyObject d, DependencyPropertyChangedEventArgs e)
        {
            if (d is TextBox textBox)
            {
                textBox.GotFocus += RemoveHint;
                textBox.LostFocus += ShowHint;

                ShowHint(textBox, null);
            }
        }

        private static void RemoveHint(object sender, RoutedEventArgs e)
        {
            if (sender is TextBox textBox && textBox.Text == GetHint(textBox))
            {
                textBox.Text = "";
                textBox.Foreground = Brushes.Black;
            }
        }

        private static void ShowHint(object sender, RoutedEventArgs e)
        {
            if (sender is TextBox textBox && string.IsNullOrEmpty(textBox.Text))
            {
                textBox.Text = GetHint(textBox);
                textBox.Foreground = Brushes.Gray;
            }
        }
    }

}
