#include "VisualTreeWatcher.h"
#include "Helpers.h"
#include "misc.h"

DWORD dwSize = sizeof(DWORD), dwOpacity = 0, dwLuminosity = 0, dwHide = 0, dwBorder = 0, dwRec = 0;

double oldSrchHeight;
Thickness oldSrchMar;
int64_t token = NULL, token_vis = NULL;

static bool g_isExplorer = false;

static bool DetectExplorer()
{
	wchar_t name[MAX_PATH];
	if (GetModuleFileNameW(NULL, name, MAX_PATH))
	{
		std::wstring path(name);
		for (auto& c : path) c = towlower(c);
		return path.find(L"explorer.exe") != std::wstring::npos;
	}
	return false;
}

VisualTreeWatcher::VisualTreeWatcher(winrt::com_ptr<IUnknown> site)
	: m_selfPtr(this, winrt::take_ownership_from_abi_t{}),
	m_XamlDiagnostics(site.as<IXamlDiagnostics>())
{
	g_isExplorer = DetectExplorer();
	this->AddRef();

	HANDLE thread = CreateThread(
		nullptr, 0,
		[](LPVOID lpParam) -> DWORD {
			auto watcher = reinterpret_cast<VisualTreeWatcher*>(lpParam);
			watcher->AdviseVisualTreeChange();
			return 0;
		},
		this, 0, nullptr);
	if (thread) {
		CloseHandle(thread);
	}
}

void VisualTreeWatcher::AdviseVisualTreeChange() {
	const auto treeService = m_XamlDiagnostics.as<IVisualTreeService3>();
	winrt::check_hresult(treeService->AdviseVisualTreeChange(this));
}

void VisualTreeWatcher::TryApplyAcrylicTint(InstanceHandle handle)
{
	try
	{
		IInspectable obj;
		winrt::check_hresult(m_XamlDiagnostics->GetIInspectableFromHandle(handle,
			reinterpret_cast<::IInspectable**>(winrt::put_abi(obj))));

		auto panel = obj.try_as<Panel>();
		if (panel)
		{
			auto bg = panel.Background();
			if (bg)
			{
				auto acrylic = bg.try_as<AcrylicBrush>();
				if (acrylic)
				{
					dwOpacity = GetVal(L"TintOpacity");
					dwLuminosity = GetVal(L"TintLuminosityOpacity");
					if (dwOpacity > 100) dwOpacity = 100;
					if (dwLuminosity > 100) dwLuminosity = 100;
					acrylic.TintOpacity(double(dwOpacity) / 100);
					acrylic.TintLuminosityOpacity(double(dwLuminosity) / 100);
					return;
				}
			}
		}
		auto border = obj.try_as<Border>();
		if (border)
		{
			auto bg = border.Background();
			if (bg)
			{
				auto acrylic = bg.try_as<AcrylicBrush>();
				if (acrylic)
				{
					dwOpacity = GetVal(L"TintOpacity");
					dwLuminosity = GetVal(L"TintLuminosityOpacity");
					if (dwOpacity > 100) dwOpacity = 100;
					if (dwLuminosity > 100) dwLuminosity = 100;
					acrylic.TintOpacity(double(dwOpacity) / 100);
					acrylic.TintLuminosityOpacity(double(dwLuminosity) / 100);
				}
			}
		}
	}
	catch (...) {}
}

HRESULT VisualTreeWatcher::OnElementStateChanged(InstanceHandle, VisualElementState, LPCWSTR) noexcept
{
	return S_OK;
}

HRESULT VisualTreeWatcher::OnVisualTreeChange(ParentChildRelation relation, VisualElement element, VisualMutationType mutationType)
{
	if (mutationType == Add)
	{
		const std::wstring_view type{ element.Type, SysStringLen(element.Type) };
		const std::wstring_view name{ element.Name, SysStringLen(element.Name) };

		if (name == L"AcrylicBorder")
		{
			dwOpacity = GetVal(L"TintOpacity");
			dwLuminosity = GetVal(L"TintLuminosityOpacity");

			if (dwOpacity > 100) dwOpacity = 100;
			if (dwLuminosity > 100) dwLuminosity = 100;
			Border acrylicBorder = FromHandle<Border>(element.Handle);
			acrylicBorder.Background().as<AcrylicBrush>().TintOpacity(double(dwOpacity) / 100);
			acrylicBorder.Background().as<AcrylicBrush>().TintLuminosityOpacity(double(dwLuminosity) / 100);
		}
		else if (name == L"BackgroundElement" && type == L"Windows.UI.Xaml.Controls.Border")
		{
			if (g_isExplorer)
			{
				dwBorder = GetVal(L"HideBorder");
				auto backElement = FromHandle<Border>(element.Handle);
				auto bg = backElement.Background();
				if (bg)
				{
					auto acrylic = bg.try_as<AcrylicBrush>();
					if (acrylic && dwBorder == 1)
						acrylic.TintOpacity(0);
				}
			}
			else
			{
				auto backElement = FromHandle<Border>(element.Handle);
				backElement.Background().as<AcrylicBrush>().TintOpacity(0);
			}
		}
		else if (type == L"StartDocked.SearchBoxToggleButton")
		{
			dwHide = GetVal(L"HideSearch");
			Control srch = FromHandle<Control>(element.Handle);
			oldSrchMar = srch.Margin();
			oldSrchHeight = srch.Height();
			if (dwHide == 1)
			{
				srch.Height(0);
				srch.Margin({0});
			}

			if (dwHide == 1) pad = srch.ActualHeight() + srch.Padding().Bottom + srch.Padding().Top + 55;
		}
		else if (name == L"RootGrid")
		{
			Grid rootContent = FromHandle<Grid>(element.Handle);
			if (GetVal(L"EditButton")==1)
			{
				AddSettingsPanel(rootContent);
			}
		}
		else if (name == L"AcrylicOverlay")
		{
			dwBorder = GetVal(L"HideBorder");
			auto acrylicOverlay = FromHandle<Border>(element.Handle);
			if (dwBorder == 1) acrylicOverlay.Background().as<SolidColorBrush>().Opacity(0);
		}
		else if (name == L"SuggestionsParentContainer" || name == L"ShowMoreSuggestions")
		{
			dwRec = GetVal(L"HideRecommended");
			auto elmnt = FromHandle<FrameworkElement>(element.Handle);
			if (dwRec == 1) elmnt.Visibility(Visibility::Collapsed);
		}
		else if (name == L"TopLevelSuggestionsListHeader")
		{
			dwRec = GetVal(L"HideRecommended");
			auto elmnt = FromHandle<FrameworkElement>(element.Handle);
			if (dwRec == 1)
			{
				elmnt.Visibility(Visibility::Collapsed);
				if (token_vis == NULL)
				{
					token_vis = elmnt.RegisterPropertyChangedCallback(UIElement::VisibilityProperty(),
						[](DependencyObject sender, DependencyProperty property)
						{
							auto element = sender.try_as<FrameworkElement>();
							element.Visibility(Visibility::Collapsed);
						});
				}
			}
		}
		else if (name == L"StartMenuPinnedList")
		{
			dwRec = GetVal(L"HideRecommended");
			if (dwRec == 1)
			{
				auto topLevelRoot = FromHandle<FrameworkElement>(relation.Parent);
				static auto suggHeader = FindDescendantByName(topLevelRoot, L"TopLevelSuggestionsListHeader").as<FrameworkElement>();
				static auto suggContainer = FindDescendantByName(topLevelRoot, L"SuggestionsParentContainer").as<FrameworkElement>();
				static auto suggBtn = FindDescendantByName(topLevelRoot, L"ShowMoreSuggestions").as<FrameworkElement>();
				auto pinList = FromHandle<FrameworkElement>(element.Handle);

				static double height = pinList.Height() + suggContainer.ActualHeight() + suggBtn.ActualHeight();
				if (token == NULL)
				{
					token = pinList.RegisterPropertyChangedCallback(FrameworkElement::HeightProperty(),
						[](DependencyObject sender, DependencyProperty property)
						{
							auto element = sender.try_as<FrameworkElement>();
							element.Height(height + pad);
						});
				}

				pinList.Height(height + pad);
				suggHeader.Visibility(Visibility::Collapsed);
				suggContainer.Visibility(Visibility::Collapsed);
				suggBtn.Visibility(Visibility::Collapsed);
			}
		}

		// Explorer-specific: system tray overflow panel
		if (g_isExplorer)
		{
			if (name == L"OverflowFlyoutBackgroundBorder")
			{
				dwOpacity = GetVal(L"TintOpacity");
				dwLuminosity = GetVal(L"TintLuminosityOpacity");
				if (dwOpacity > 100) dwOpacity = 100;
				if (dwLuminosity > 100) dwLuminosity = 100;
				auto border = FromHandle<Border>(element.Handle);
				auto bg = border.Background();
				if (bg)
				{
					auto acrylic = bg.try_as<AcrylicBrush>();
					if (acrylic)
					{
						acrylic.TintOpacity(double(dwOpacity) / 100);
						acrylic.TintLuminosityOpacity(double(dwLuminosity) / 100);
					}
				}
			}
			else if (type == L"SystemTray.NotificationAreaOverflow")
			{
				TryApplyAcrylicTint(element.Handle);
			}
		}

		// Generic catch-all for any element with acrylic brush (both processes)
		if (type.find(L"Border") != std::wstring_view::npos
			|| type.find(L"Panel") != std::wstring_view::npos
			|| type.find(L"Grid") != std::wstring_view::npos)
		{
			TryApplyAcrylicTint(element.Handle);
		}
	}
	return S_OK;
}
