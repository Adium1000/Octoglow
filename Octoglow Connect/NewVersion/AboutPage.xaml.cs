using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml;
namespace OctoglowSender;
public sealed partial class AboutPage : Page
{
    private const string MitLicenseBody =
        "Permission is hereby granted, free of charge, to any person obtaining a copy " +
        "of this software and associated documentation files (the \"Software\"), to deal " +
        "in the Software without restriction, including without limitation the rights to " +
        "use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies " +
        "of the Software, and to permit persons to whom the Software is furnished to do so, " +
        "subject to the following conditions:\n\nThe above copyright notice and this permission " +
        "notice shall be included in all copies or substantial portions of the Software.\n\n" +
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, " +
        "INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR " +
        "PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE " +
        "FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR " +
        "OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER " +
        "DEALINGS IN THE SOFTWARE.";
    public AboutPage()
    {
        InitializeComponent();
        AppStrings.LanguageChanged += (_, _) => DispatcherQueue.TryEnqueue(ApplyLanguage);
        ApplyLanguage();
    }
    private void ApplyInitialDescription() => DescriptionText.Text = AppStrings.IsEnglish
        ? "Send your current activity to your Octoglow device."
        : "Trimite activitatea curentă către dispozitivul tău Octoglow.";

    private async void SdkLicense_Click(object sender, RoutedEventArgs e)
    {
        await ShowLicenseAsync("SCSSdkClient / scs-sdk-plugin", "Copyright (c) 2014 Hans");
    }
    private async void AppLicense_Click(object sender, RoutedEventArgs e)
    {
        await ShowLicenseAsync("Octoglow Connect", "Copyright (c) 2026 Octoglow");
    }
    private async Task ShowLicenseAsync(string title, string copyright)
    {
        var dialog = new ContentDialog
        {
            XamlRoot = XamlRoot,
            Title = title,
            CloseButtonText = AppStrings.IsEnglish ? "Close" : "Închide",
            Content = new ScrollViewer
            {
                MaxHeight = 480,
                Content = new TextBlock { Text = $"The MIT License (MIT)\n\n{copyright}\n\n{MitLicenseBody}", TextWrapping = TextWrapping.Wrap }
            }
        };
        await dialog.ShowAsync();
    }
    private void ApplyLanguage()
    {
        AppLicenseSummaryText.Text = AppStrings.IsEnglish
            ? "MIT License · Copyright (c) 2026 Octoglow"
            : "Licența MIT · Copyright (c) 2026 Octoglow";
        AppLicenseButton.Content = AppStrings.IsEnglish ? "View application license" : "Vezi licența aplicației";
        ThirdPartyTitleText.Text = AppStrings.IsEnglish ? "Third-party licenses" : "Licențe terțe";
        DescriptionText.Text = AppStrings.IsEnglish ? "Send your current activity to your Octoglow device." : "Trimite activitatea curentă către dispozitivul tău Octoglow.";
        SdkLicenseButton.Content = AppStrings.IsEnglish ? "View SDK license" : "Vezi licența SDK";
    }
}
