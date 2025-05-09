/////////////////////////////////////////////////////////////////////////////
// Name:        src/qt/choice.cpp
// Author:      Peter Most, Mariano Reingart
// Copyright:   (c) 2010 wxWidgets dev team
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"

#if wxUSE_CHOICE

#include "wx/choice.h"
#include "wx/qt/private/winevent.h"

#include <QtWidgets/QComboBox>
#include <QtCore/QSortFilterProxyModel>

namespace
{

class LexicalSortProxyModel : public QSortFilterProxyModel
{
public:
    explicit LexicalSortProxyModel(QObject* owner) : QSortFilterProxyModel(owner) {}

    bool lessThan( const QModelIndex &left, const QModelIndex &right ) const override
    {
        const QVariant leftData = sourceModel()->data( left );
        const QVariant rightData = sourceModel()->data( right );

        const bool isStringType =
#if QT_VERSION_MAJOR >= 6
            leftData.metaType().id() == QMetaType::QString;
#else
            leftData.type() == QVariant::String;
#endif

        if ( !isStringType )
            return false;

        int insensitiveResult = QString::compare(
                leftData.value<QString>(),
                rightData.value<QString>(),
                Qt::CaseInsensitive );

        if ( insensitiveResult == 0 )
        {
            return QString::compare( leftData.value<QString>(),
                                     rightData.value<QString>() ) < 0;
        }

        return insensitiveResult < 0;
    }
};

class wxQtChoice : public wxQtEventSignalHandler< QComboBox, wxChoice >
{
public:
    wxQtChoice( wxWindow *parent, wxChoice *handler );

private:
    void activated(int index);
};

wxQtChoice::wxQtChoice( wxWindow *parent, wxChoice *handler )
    : wxQtEventSignalHandler< QComboBox, wxChoice >( parent, handler )
{
    // the activated signal is overloaded, the following explicit cast is needed:
    connect(this, static_cast<void (QComboBox::*)(int index)>(&QComboBox::activated),
            this, &wxQtChoice::activated);
}

void wxQtChoice::activated(int WXUNUSED(index))
{
    wxChoice *handler = GetHandler();
    if ( handler )
        handler->SendSelectionChangedEvent(wxEVT_CHOICE);
}

} // anonymous namespace

void wxChoice::QtInitSort( QComboBox *combo )
{
    QSortFilterProxyModel *proxyModel = new LexicalSortProxyModel(combo);
    proxyModel->setSourceModel(combo->model());
    combo->model()->setParent(proxyModel);
    combo->setModel(proxyModel);
}


wxChoice::wxChoice( wxWindow *parent, wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        int n, const wxString choices[],
        long style,
        const wxValidator& validator,
        const wxString& name )
{
    Create( parent, id, pos, size, n, choices, style, validator, name );
}


wxChoice::wxChoice( wxWindow *parent, wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        const wxArrayString& choices,
        long style,
        const wxValidator& validator,
        const wxString& name )
{
    Create( parent, id, pos, size, choices, style, validator, name );
}


bool wxChoice::Create( wxWindow *parent, wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        const wxArrayString& choices,
        long style,
        const wxValidator& validator,
        const wxString& name )
{
    return Create( parent, id, pos, size, choices.size(), choices.size() ? &choices[ 0 ] : nullptr, style,
        validator, name );
}


bool wxChoice::Create( wxWindow *parent, wxWindowID id,
        const wxPoint& pos,
        const wxSize& size,
        int n, const wxString choices[],
        long style,
        const wxValidator& validator,
        const wxString& name )
{
    m_qtWindow = new wxQtChoice( parent, this );

    QtInitSort( GetQComboBox() );

    while ( n-- > 0 )
    {
        GetQComboBox()->addItem( wxQtConvertString( *choices++ ));
    }

    return wxChoiceBase::Create( parent, id, pos, size, style, validator, name );
}

QComboBox* wxChoice::GetQComboBox() const
{
    return static_cast<QComboBox*>(m_qtWindow);
}

wxSize wxChoice::DoGetBestSize() const
{
    wxSize basesize = wxChoiceBase::DoGetBestSize();
    wxSize size = wxControl::DoGetBestSize();
    // mix calculated size by wx base prioritizing qt hint (max):
    if (size.GetWidth() < basesize.GetWidth())
        size.SetWidth(basesize.GetWidth());
    if (size.GetHeight() < basesize.GetHeight())
        size.SetHeight(basesize.GetHeight());
    return size;
}


unsigned wxChoice::GetCount() const
{
    return GetQComboBox()->count();
}

wxString wxChoice::GetString(unsigned int n) const
{
    wxCHECK_MSG(n < GetCount(), wxString(), "invalid index");

    return wxQtConvertString( GetQComboBox()->itemText(n) );
}

void wxChoice::SetString(unsigned int n, const wxString& s)
{
    GetQComboBox()->setItemText(n, wxQtConvertString(s));
}


void wxChoice::SetSelection(int n)
{
    wxQtEnsureSignalsBlocked blocker(GetQComboBox());
    GetQComboBox()->setCurrentIndex(n);
}

int wxChoice::GetSelection() const
{
    return GetQComboBox()->currentIndex();
}


int wxChoice::DoInsertItems(const wxArrayStringsAdapter & items,
                  unsigned int pos,
                  void **clientData,
                  wxClientDataType type)
{
    InvalidateBestSize();

    // Hack: to avoid resorting the model many times in DoInsertOneItem(),
    // which will be called for each item from DoInsertItemsInLoop(), reset the
    // wxCB_SORT style if we have it temporarily and only sort once at the end.
    bool wasSorted = false;
    if ( IsSorted() )
    {
        wasSorted = true;
        ToggleWindowStyle(wxCB_SORT);
    }

    int n = DoInsertItemsInLoop(items, pos, clientData, type);

    if ( wasSorted )
    {
        // Restore the flag turned off above.
        ToggleWindowStyle(wxCB_SORT);

        // And actually sort items now.
        GetQComboBox()->model()->sort(0);
    }

    return n;
}

int wxChoice::DoInsertOneItem(const wxString& item, unsigned int pos)
{
    // Maintain unselected state
    const bool unselected = GetQComboBox()->currentIndex() == -1;

    GetQComboBox()->insertItem(pos, wxQtConvertString(item));

    if ( IsSorted() )
        GetQComboBox()->model()->sort(0);

    if ( unselected )
        GetQComboBox()->setCurrentIndex(-1);

    return pos;
}

void wxChoice::DoSetItemClientData(unsigned int n, void *clientData)
{
    QVariant variant = QVariant::fromValue(clientData);
    GetQComboBox()->setItemData(n, variant);
}

void *wxChoice::DoGetItemClientData(unsigned int n) const
{
    QVariant variant = GetQComboBox()->itemData(n);
    return variant.value<void *>();
}


void wxChoice::DoClear()
{
    GetQComboBox()->clear();
}

void wxChoice::DoDeleteOneItem(unsigned int pos)
{
    const int selection = GetSelection();

    if ( selection >= 0 && static_cast<unsigned int>(selection) == pos )
    {
        SetSelection( wxNOT_FOUND );
    }
    GetQComboBox()->removeItem(pos);
}

#endif // wxUSE_CHOICE
