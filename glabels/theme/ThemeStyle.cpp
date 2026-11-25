//  ThemeStyle.cpp
//
//  Copyright (C) 2026  gLabels Neo contributors
//
//  This file is part of gLabels-qt.
//
//  gLabels-qt is free software: you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation, either version 3 of the License, or
//  (at your option) any later version.
//
//  gLabels-qt is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with gLabels-qt.  If not, see <http://www.gnu.org/licenses/>.
//


#include "theme/ThemeStyle.hpp"

#include "theme/Decorator.hpp"
#include "theme/ThemeIcons.hpp"

#include <QDebug>
#include <QRegularExpression>


namespace
{

        //
        // The stylesheet template.  Sections follow SPEC.md section 8; sizes follow 4.3.
        //
        // Split into several literals: MSVC rejects a single string literal over 16380 bytes (C2026).
        const char* const styleTemplateParts[] = {
        R"QSS(
/* ===================================================================== base surfaces */

QDialog, QMessageBox, QWizard {
        background: @panel;
}
QWizard > QWidget {
        background: transparent;
}
QMainWindow {
        background: @bg;
}
QMainWindow::separator {
        background: @line;
        width: 1px;
        height: 1px;
}
QSplitter::handle {
        background: @line;
}
QSplitter::handle:horizontal {
        width: 1px;
}
QSplitter::handle:vertical {
        height: 1px;
}
QLabel {
        background: transparent;
}
QLabel:disabled {
        color: @ink/40;
}

/* ====================================================================== tooltips (D10) */

QToolTip {
        background: @panel;
        color: @ink;
        border: 1px solid @line2;
        padding: 6px 8px;
        font-size: 12px;
}

/* ======================================================================= push buttons */

QPushButton {
        background: @panel;
        color: @ink;
        border: 1px solid @line2;
        border-radius: 6px;
        padding: 5px 14px;
        min-height: 18px;
        font-weight: 500;
}
QPushButton:hover {
        background: @hover;
}
QPushButton:pressed {
        background: @pressed;
}
QPushButton:checked {
        background: @pressed;
        border-color: @accent;
        color: @accent;
}
QPushButton:focus {
        border: 2px solid @accent;
        padding: 4px 13px;
}
QPushButton:disabled {
        background: @panel;
        border-color: @line;
        color: @ink/40;
}
QPushButton:default, QPushButton[role="primary"] {
        background: @accent;
        border: 1px solid @accent;
        color: @onGrad;
}
QPushButton:default:hover, QPushButton[role="primary"]:hover {
        background: @accentHover;
        border-color: @accentHover;
}
QPushButton:default:pressed, QPushButton[role="primary"]:pressed {
        background: @accentPressed;
        border-color: @accentPressed;
}
QPushButton:default:focus, QPushButton[role="primary"]:focus {
        border: 2px solid @focusOnAccent;
        padding: 4px 13px;
}
QPushButton:default:disabled, QPushButton[role="primary"]:disabled {
        background: @accent/40;
        border-color: @accent/0;
        color: @onGrad;
}
QPushButton:flat {
        background: transparent;
        border-color: transparent;
}
QPushButton:flat:hover {
        background: @hover;
}
QPushButton:flat:pressed, QPushButton:flat:checked {
        background: @pressed;
        border-color: transparent;
        color: @accent;
}
QCommandLinkButton {
        background: @panel;
        color: @ink;
        border: 1px solid @line;
        border-radius: 10px;
        padding: 12px 14px;
        text-align: left;
}
QCommandLinkButton:hover {
        background: @hover;
        border-color: @line2;
}
QCommandLinkButton:default {
        background: @panel;
        border: 1px solid @line;
        color: @ink;
}
QCommandLinkButton:checked, QCommandLinkButton:pressed {
        background: @pressed;
        border: 1px solid @accent;
        color: @ink;
}
QCommandLinkButton:focus, QCommandLinkButton:default:focus {
        border: 2px solid @accent;
        padding: 11px 13px;
}
QCommandLinkButton:disabled {
        background: @panel;
        border-color: @line;
        color: @ink/40;
}
QPushButton::menu-indicator {
        image: url("@imgChevronDown");
        subcontrol-origin: padding;
        subcontrol-position: center right;
        width: 12px;
        height: 12px;
        right: 4px;
}

/* ======================================================================= tool buttons */

QToolButton {
        background: transparent;
        color: @ink;
        border: 1px solid transparent;
        border-radius: 6px;
        padding: 3px;
}
QToolButton:hover {
        background: @hover;
}
QToolButton:pressed, QToolButton:checked {
        background: @pressed;
        color: @accent;
}
QToolButton:focus {
        border: 2px solid @accent;
        padding: 2px;
}
QToolButton:disabled {
        color: @ink/40;
        background: transparent;
}
QToolButton[popupMode="1"] {
        padding-right: 16px;
}
QToolButton::menu-button {
        border: none;
        border-left: 1px solid @line;
        width: 14px;
}
QToolButton::menu-indicator, QToolButton::menu-arrow {
        image: url("@imgChevronDown");
        width: 10px;
        height: 10px;
}
QToolButton::menu-indicator {
        subcontrol-origin: padding;
        subcontrol-position: bottom right;
}

/* ===================================================== line edits, spins, combos, text */

QLineEdit, QAbstractSpinBox, QComboBox, QPlainTextEdit, QTextEdit {
        background: @field;
        color: @ink;
        border: 1px solid @line2;
        border-radius: 6px;
        selection-background-color: @accent;
        selection-color: @onGrad;
}
QLineEdit, QAbstractSpinBox, QComboBox {
        padding: 3px 8px;
        min-height: 18px;
}
QPlainTextEdit, QTextEdit {
        padding: 4px 6px;
}
QLineEdit:hover, QAbstractSpinBox:hover, QComboBox:hover, QPlainTextEdit:hover, QTextEdit:hover {
        border-color: @ink3;
}
QLineEdit:focus, QAbstractSpinBox:focus, QComboBox:focus, QPlainTextEdit:focus, QTextEdit:focus {
        border: 2px solid @accent;
}
QLineEdit:focus, QAbstractSpinBox:focus, QComboBox:focus {
        padding: 2px 7px;
}
QPlainTextEdit:focus, QTextEdit:focus {
        padding: 3px 5px;
}
QLineEdit:disabled, QAbstractSpinBox:disabled, QComboBox:disabled, QPlainTextEdit:disabled, QTextEdit:disabled {
        background: @chrome;
        border-color: @line;
        color: @ink/40;
}
QLineEdit:read-only, QPlainTextEdit:read-only, QTextEdit:read-only {
        background: @bg;
}
QTextBrowser {
        background: @panel;
        border: 1px solid @line;
        border-radius: 8px;
}

QComboBox {
        padding-right: 24px;
}
QComboBox:focus {
        padding-right: 23px;
}
QComboBox::drop-down {
        subcontrol-origin: padding;
        subcontrol-position: center right;
        width: 22px;
        border: none;
        background: transparent;
}
QComboBox::down-arrow {
        image: url("@imgChevronDown");
        width: 12px;
        height: 12px;
}
QComboBox::down-arrow:disabled {
        image: url("@imgChevronDownDisabled");
}
QComboBox {
        combobox-popup: 0;
}
QComboBox QAbstractItemView, QAbstractItemView[popupList="true"] {
        background: @panel;
        color: @ink;
        border: 1px solid @line2;
        border-radius: 0px;
        padding: 0px;
        outline: 0;
        selection-background-color: @pressed;
        selection-color: @ink;
}
QComboBox QAbstractItemView::item, QAbstractItemView[popupList="true"]::item {
        min-height: 24px;
        padding: 0px 8px;
        border: none;
}
QComboBox QAbstractItemView::item:selected, QAbstractItemView[popupList="true"]::item:selected,
QComboBox QAbstractItemView::item:hover, QAbstractItemView[popupList="true"]::item:hover {
        background: @pressed;
        color: @ink;
}

QAbstractSpinBox {
        padding-left: 6px;
        padding-right: 16px;
}
QAbstractSpinBox:focus {
        padding-left: 5px;
        padding-right: 15px;
}
QAbstractSpinBox[role="ribbonField"] {
        padding-left: 5px;
        padding-right: 12px;
}
QAbstractSpinBox[role="ribbonField"]:focus {
        padding-left: 4px;
        padding-right: 11px;
}
QAbstractSpinBox[role="ribbonField"]::up-button, QAbstractSpinBox[role="ribbonField"]::down-button {
        width: 12px;
}
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button {
        subcontrol-origin: border;
        width: 16px;
        border: none;
        background: transparent;
}
QAbstractSpinBox::up-button {
        subcontrol-position: top right;
        border-top-right-radius: 6px;
}
QAbstractSpinBox::down-button {
        subcontrol-position: bottom right;
        border-bottom-right-radius: 6px;
}
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover {
        background: @hover;
}
QAbstractSpinBox::up-arrow {
        image: url("@imgChevronUp");
        width: 10px;
        height: 10px;
}
QAbstractSpinBox::down-arrow {
        image: url("@imgChevronDown");
        width: 10px;
        height: 10px;
}
QAbstractSpinBox::up-arrow:disabled, QAbstractSpinBox::up-arrow:off {
        image: url("@imgChevronUpDisabled");
}
QAbstractSpinBox::down-arrow:disabled, QAbstractSpinBox::down-arrow:off {
        image: url("@imgChevronDownDisabled");
}

/* ================================================================= checks and radios */

QCheckBox, QRadioButton {
        spacing: 8px;
        color: @ink;
        background: transparent;
}
QCheckBox:disabled, QRadioButton:disabled {
        color: @ink/40;
}
QCheckBox::indicator, QGroupBox::indicator, QAbstractItemView::indicator {
        width: 14px;
        height: 14px;
        border: 1px solid @line2;
        border-radius: 4px;
        background: @panel;
}
QCheckBox::indicator:hover, QGroupBox::indicator:hover, QAbstractItemView::indicator:hover {
        border-color: @accent;
}
QCheckBox::indicator:checked, QGroupBox::indicator:checked, QAbstractItemView::indicator:checked {
        background: @accent;
        border-color: @accent;
        image: url("@imgCheck");
}
QCheckBox::indicator:indeterminate, QAbstractItemView::indicator:indeterminate {
        background: @accent;
        border-color: @accent;
        image: url("@imgMinus");
}
QCheckBox::indicator:disabled, QGroupBox::indicator:disabled, QAbstractItemView::indicator:disabled {
        background: @chrome;
        border-color: @line;
}
QCheckBox::indicator:checked:disabled, QGroupBox::indicator:checked:disabled {
        background: @accent/40;
        border-color: @accent/0;
}
QCheckBox:focus, QRadioButton:focus {
        outline: none;
}
QCheckBox::indicator:focus {
        border: 2px solid @accent;
}
QRadioButton::indicator:focus {
        border: none;
        image: url("@imgRadioOffHover");
}
QRadioButton::indicator:checked:focus {
        image: url("@imgRadioOn");
}
QRadioButton::indicator {
        width: 16px;
        height: 16px;
        border: none;
        image: url("@imgRadioOff");
}
QRadioButton::indicator:hover {
        image: url("@imgRadioOffHover");
}
QRadioButton::indicator:checked {
        image: url("@imgRadioOn");
}
QRadioButton::indicator:disabled {
        image: url("@imgRadioOffDisabled");
}
QRadioButton::indicator:checked:disabled {
        image: url("@imgRadioOnDisabled");
}
)QSS",
        R"QSS(

/* ======================================================= group boxes as cards (6.4) */
/* The card surface and its header band are painted by theme::Decorator.            */

QGroupBox {
        background: transparent;
        border: none;
        margin: 0px;
        padding: @cardTop 6px 4px 6px;
        font-weight: 600;
        color: @ink;
}
QGroupBox::title {
        subcontrol-origin: margin;
        subcontrol-position: top left;
        padding: 0px 14px;
        margin-top: @cardTitleTop;
        color: @ink;
        background: transparent;
}
QGroupBox:flat {
        padding: 22px 0px 0px 0px;
}
QGroupBox::title:flat {
        margin-top: 0px;
        padding: 0px;
}
QGroupBox::title:disabled {
        color: @ink/40;
}

/* ================================================================== tabs (QTabWidget) */

QTabWidget::pane {
        background: @panel;
        border: 1px solid @line;
        border-radius: 10px;
        top: -1px;
}
QTabBar {
        background: transparent;
}
QTabBar::tab {
        background: transparent;
        color: @ink2;
        border: none;
        border-bottom: 2px solid transparent;
        padding: 7px 14px;
        margin-right: 2px;
}
QTabBar::tab:hover {
        color: @ink;
        background: @hover;
}
QTabBar::tab:selected {
        color: @ink;
        border-bottom: 2px solid @accent;
}
QTabBar::tab:disabled {
        color: @ink/40;
}
QTabBar::tab:focus {
        color: @accent;
}
QTabBar QToolButton {
        background: @panel;
        border: 1px solid @line;
}

/* ============================================================ tables, lists and trees */

QAbstractItemView {
        background: @panel;
        alternate-background-color: @bg;
        color: @ink;
        border: 1px solid @line;
        border-radius: 8px;
        gridline-color: @line;
        selection-background-color: @pressed;
        selection-color: @ink;
        outline: 0;
}
QAbstractItemView:focus {
        border-color: @accent;
}
QAbstractItemView:disabled {
        color: @ink/40;
}
QAbstractItemView::item {
        padding: 2px 4px;
}
QAbstractItemView::item:hover {
        background: @hover;
}
QAbstractItemView::item:selected {
        background: @pressed;
        color: @ink;
}
QHeaderView {
        background: transparent;
        border: none;
}
QHeaderView::section {
        background: @chrome;
        color: @ink2;
        border: none;
        border-bottom: 1px solid @line;
        border-right: 1px solid @line;
        padding: 6px 8px;
        font-size: 12px;
}
QHeaderView::section:last {
        border-right: none;
}
QHeaderView::section:checked {
        color: @ink;
}
QTableCornerButton::section {
        background: @chrome;
        border: none;
        border-bottom: 1px solid @line;
        border-right: 1px solid @line;
}
QHeaderView::up-arrow {
        image: url("@imgChevronUp");
        width: 10px;
        height: 10px;
}
QHeaderView::down-arrow {
        image: url("@imgChevronDown");
        width: 10px;
        height: 10px;
}

/* ======================================================================== scroll bars */

QScrollBar:vertical {
        background: transparent;
        width: 12px;
        margin: 0px;
        border: none;
}
QScrollBar:horizontal {
        background: transparent;
        height: 12px;
        margin: 0px;
        border: none;
}
QScrollBar::handle:vertical {
        background: @line2;
        border-radius: 4px;
        min-height: 28px;
        margin: 2px;
}
QScrollBar::handle:horizontal {
        background: @line2;
        border-radius: 4px;
        min-width: 28px;
        margin: 2px;
}
QScrollBar::handle:hover {
        background: @ink3;
}
QScrollBar::add-line, QScrollBar::sub-line {
        width: 0px;
        height: 0px;
        border: none;
        background: none;
}
QScrollBar::add-page, QScrollBar::sub-page {
        background: none;
}
QAbstractScrollArea::corner {
        background: transparent;
        border: none;
}

/* ============================================================== menus (context, popups) */

QMenuBar {
        background: @chrome;
        color: @ink;
        border-bottom: 1px solid @line;
}
QMenuBar::item {
        background: transparent;
        padding: 6px 10px;
        border-radius: 6px;
}
QMenuBar::item:selected {
        background: @hover;
}
QMenuBar::item:pressed {
        background: @pressed;
}
QMenu {
        background: @panel;
        color: @ink;
        border: 1px solid @line2;
        padding: 6px;
}
QMenu::item {
        background: transparent;
        padding: 6px 28px 6px 12px;
        border-radius: 6px;
}
QMenu::item:selected {
        background: @pressed;
        color: @ink;
}
QMenu::item:disabled {
        color: @ink/40;
        background: transparent;
}
QMenu::icon {
        padding-left: 8px;
}
QMenu::separator {
        height: 1px;
        background: @line;
        margin: 6px 8px;
}
QMenu::indicator {
        width: 14px;
        height: 14px;
        padding-left: 6px;
}
QMenu::indicator:checked {
        image: url("@imgCheckAccent");
}
QMenu::right-arrow {
        image: url("@imgChevronRight");
        width: 12px;
        height: 12px;
        padding-right: 6px;
}
QMenu::section {
        color: @ink3;
        font-size: 11px;
        font-weight: 600;
        padding: 8px 12px 4px 12px;
}

/* ===================================================================== toolbars, docks */

QToolBar {
        background: @chrome;
        border: none;
        border-bottom: 1px solid @line;
        padding: 2px 6px;
        spacing: 2px;
}
QToolBar::separator {
        background: @line;
        width: 1px;
        margin: 4px 6px;
}
QDockWidget {
        color: @ink;
}
QDockWidget::title {
        background: @chrome;
        padding: 6px 8px;
        border-bottom: 1px solid @line;
}

/* ===================================================================== status bar (6.6) */

QStatusBar, QStatusBar[role="ribbonStatusBar"] {
        background: @chrome;
        color: @ink2;
        border-top: 1px solid @line;
        min-height: 26px;
        font-size: 12px;
}
QStatusBar::item {
        border: none;
}
QStatusBar QLabel {
        color: @ink2;
        font-size: 12px;
}

/* =========================================================== sliders and progress bars */

QSlider::groove:horizontal {
        height: 4px;
        background: @line2;
        border-radius: 2px;
}
QSlider::sub-page:horizontal {
        background: @accent;
        border-radius: 2px;
}
QSlider::add-page:horizontal {
        background: @line2;
        border-radius: 2px;
}
QSlider::handle:horizontal {
        background: @panel;
        border: 2px solid @accent;
        width: 10px;
        height: 10px;
        margin: -5px 0px;
        border-radius: 7px;
}
QSlider::groove:vertical {
        width: 4px;
        background: @line2;
        border-radius: 2px;
}
QSlider::handle:vertical {
        background: @panel;
        border: 2px solid @accent;
        width: 10px;
        height: 10px;
        margin: 0px -5px;
        border-radius: 7px;
}
QSlider::handle:disabled {
        border-color: @line2;
}
QSlider[role="zoomSlider"] {
        min-width: 120px;
        max-height: 16px;
}
QProgressBar {
        background: @hover;
        color: @ink;
        border: none;
        border-radius: 4px;
        text-align: center;
        min-height: 8px;
        font-size: 12px;
}
QProgressBar::chunk {
        background: @accent;
        border-radius: 4px;
}

/* ================================================================= ribbon (4.3, 5.1) */

*[role="ribbonBar"] {
        background: @chrome;
}
*[role="ribbonTabStrip"] {
        background: @chrome;
        border-bottom: 1px solid @line;
        min-height: @mTabStrip;
}
QAbstractButton[role="ribbonTab"] {
        background: transparent;
        color: @ink2;
        border: none;
        border-bottom: 3px solid transparent;
        border-top-left-radius: 6px;
        border-top-right-radius: 6px;
        padding: 0px 16px;
        min-height: @mTabInner;
        font-size: 13px;
}
QAbstractButton[role="ribbonTab"]:hover {
        color: @ink;
        background: @hover;
}
QAbstractButton[role="ribbonTab"]:checked {
        background: @ribbon;
        color: @ink;
        font-weight: 600;
        border-bottom: 3px solid @accent;
}
QAbstractButton[role="ribbonContextTab"] {
        background: @ctx;
        color: @ink;
        border: none;
        border-top: 2px solid @ctxLine;
        border-top-left-radius: 6px;
        border-top-right-radius: 6px;
        margin-left: 14px;
        padding: 0px 16px;
        min-height: @mTabInner;
        font-size: 13px;
}
QAbstractButton[role="ribbonContextTab"]:checked {
        font-weight: 600;
        border-bottom: 3px solid @accent;
}
QAbstractButton[role="fileButton"] {
        background: @grad;
        color: @onGrad;
        border: none;
        border-radius: 8px;
        padding: 0px 20px;
        min-height: 28px;
        max-height: 28px;
        font-size: 13px;
        font-weight: 600;
}
QAbstractButton[role="fileButton"]:hover, QAbstractButton[role="fileButton"]:checked {
        background: @gradHover;
}
QAbstractButton[role="fileButton"]:focus {
        border: 2px solid @focusOnAccent;
}
*[role="quickAccess"] {
        background: transparent;
}
QToolButton[role="quickAccessButton"] {
        min-width: 26px;
        max-width: 26px;
        min-height: 26px;
        max-height: 26px;
        padding: 0px;
}
*[role="ribbonBody"] {
        background: @ribbon;
        border-bottom: 1px solid @line;
}
*[role="ribbonGroup"] {
        background: transparent;
        border: none;
        border-right: 1px solid @line;
        padding: 0px @mGroupPad;
}
QLabel[role="ribbonGroupName"] {
        color: @ink3;
        font-size: 12px;
        min-height: @mGroupName;
        max-height: @mGroupName;
        qproperty-alignment: AlignCenter;
}
QToolButton[role="ribbonLarge"], QToolButton[role="ribbonPrimary"] {
        min-width: @mLargeW;
        min-height: @mLargeH;
        max-height: @mLargeH;
        border-radius: 8px;
        padding: 4px 2px;
        font-size: 12px;
        color: @ink;
        qproperty-iconSize: 30px 30px;
}
QToolButton[role="ribbonSmall"], QToolButton[role="ribbonDrop"] {
        min-height: @mSmallH;
        max-height: @mSmallH;
        border-radius: 6px;
        padding: 0px 6px;
        font-size: 12px;
        color: @ink;
        qproperty-iconSize: 16px 16px;
}
QToolButton[role="ribbonDrop"] {
        padding-right: 18px;
}
QAbstractButton[role="ribbonIcon"], QAbstractButton[role="ribbonToggle"] {
        min-width: @mIconBtn;
        max-width: @mIconBtn;
        min-height: @mIconBtn;
        max-height: @mIconBtn;
        border-radius: 6px;
        padding: 0px;
        qproperty-iconSize: 16px 16px;
}
QToolButton[role="ribbonLarge"]:checked, QToolButton[role="ribbonSmall"]:checked,
QAbstractButton[role="ribbonIcon"]:checked, QAbstractButton[role="ribbonToggle"]:checked {
        background: @pressed;
        color: @accent;
}
QToolButton[role="ribbonPrimary"], QToolButton[primary="true"] {
        background: @grad;
        color: @onGrad;
        font-weight: 600;
}
QToolButton[role="ribbonPrimary"]:hover, QToolButton[primary="true"]:hover {
        background: @gradHover;
}
QToolButton[role="ribbonPrimary"]:disabled, QToolButton[primary="true"]:disabled {
        background: @accent/40;
        color: @onGrad;
}
QToolButton[role="ribbonGroupOverflow"] {
        min-width: @mLargeW;
        min-height: @mLargeH;
        border-radius: 8px;
        font-size: 12px;
}
*[role="ribbonToggleGroup"] {
        background: @panel;
        border: 1px solid @line2;
        border-radius: 6px;
}
QComboBox[role="ribbonField"], QAbstractSpinBox[role="ribbonField"], QLineEdit[role="ribbonField"] {
        min-height: 18px;
        max-height: @mField;
        font-size: 12px;
        padding-top: 2px;
        padding-bottom: 2px;
}
QLabel[role="ribbonCaption"], QLabel[role="ribbonInfo"] {
        color: @ink2;
        font-size: 12px;
}
)QSS",
        R"QSS(

/* ================================================== backstage, welcome, content panel */

*[role="backstage"], *[role="welcome"], *[role="page"] {
        background: @bg;
}
*[role="backstageNav"] {
        background: @chrome;
        border: none;
        border-right: 1px solid @line;
        min-width: 240px;
        max-width: 240px;
}
QAbstractButton[role="backstageNavItem"] {
        background: transparent;
        color: @ink;
        border: none;
        border-radius: 6px;
        padding: 0px 12px;
        min-height: 36px;
        text-align: left;
        font-size: 13px;
}
QAbstractButton[role="backstageNavItem"]:hover {
        background: @hover;
}
QAbstractButton[role="backstageNavItem"]:checked {
        background: @pressed;
        color: @accent;
        font-weight: 600;
}
QFrame[role="backstageSeparator"] {
        background: @line;
        border: none;
        min-height: 1px;
        max-height: 1px;
}
*[role="backstagePane"] {
        background: @bg;
}
QAbstractButton[role="commandLink"] {
        background: transparent;
        color: @ink;
        border: 1px solid transparent;
        border-radius: 8px;
        padding: 10px 12px;
        text-align: left;
        font-size: 13px;
}
QAbstractButton[role="commandLink"]:hover {
        background: @hover;
        border-color: @line;
}
QAbstractButton[role="commandLink"]:focus {
        border: 2px solid @accent;
}
QAbstractItemView[role="recentList"] {
        background: transparent;
        border: none;
}
QAbstractItemView[role="recentList"]::item {
        min-height: 32px;
        border-radius: 6px;
}
*[role="contentPanel"] {
        background: @panel;
        border: none;
        border-left: 1px solid @line;
}
*[role="contentPanel"] QScrollArea {
        background: @panel;
        border: none;
}
*[role="contentPanel"] QScrollArea > QWidget > QWidget {
        background: @panel;
}
*[role="contentPanelHeader"] {
        background: @panel;
        border-bottom: 1px solid @line;
        min-height: 40px;
        max-height: 40px;
}
QLabel[role="panelTitle"] {
        color: @ink;
        font-size: 13px;
        font-weight: 600;
}
*[role="fieldChip"] {
        background: @tint;
        color: @accent;
        border: none;
        border-radius: 4px;
        padding: 1px 6px;
        font-family: "JetBrains Mono", Consolas, monospace;
        font-size: 12px;
}
QLabel[role="statusText"] {
        color: @ink2;
        font-size: 12px;
}

/* ============================================================================== pages */

QLabel[role="welcomeTitle"] {
        color: @ink;
        font-size: 24px;
        font-weight: 700;
}
QLabel[role="pageTitle"] {
        color: @ink;
        font-size: 20px;
        font-weight: 700;
}
QFrame[role="card"] {
        background: @panel;
        border: 1px solid @line;
        border-radius: 10px;
}
*[role="cardHeader"] {
        background: @chrome;
        color: @ink;
        border: none;
        border-bottom: 1px solid @line;
        min-height: @cardHeader;
        font-size: 13px;
        font-weight: 600;
}
QLabel[role="caption"] {
        color: @ink3;
        font-size: 12px;
}
*[role="workWell"] {
        background: @work;
        border: none;
}
QLabel[role="pill"] {
        background: @hover;
        color: @ink2;
        border-radius: 10px;
        padding: 2px 10px;
        font-size: 12px;
}
QLabel[role="pill"][tone="ok"] {
        background: @okTint;
        color: @ok;
}
QLabel[role="pill"][tone="warn"] {
        background: @warnTint;
        color: @warn;
}
*[role="popover"] {
        background: @panel;
        border: 1px solid @line2;
        border-radius: 0px;
}
QLabel[role="popoverSection"] {
        color: @ink3;
        font-size: 11px;
        font-weight: 600;
}
QToolButton[role="swatch"] {
        min-width: 20px;
        max-width: 20px;
        min-height: 20px;
        max-height: 20px;
        padding: 0px;
        border: 1px solid @line2;
        border-radius: 4px;
}
QToolButton[role="swatch"]:hover, QToolButton[role="swatch"]:focus {
        border: 2px solid @accent;
}

/* ============================================================================ dialogs */

QLabel[role="dialogHeading"] {
        color: @ink;
        font-size: 15px;
        font-weight: 600;
}
QLabel[role="dialogBody"] {
        color: @ink2;
        font-size: 13px;
}
QMessageBox QLabel#qt_msgbox_label {
        color: @ink;
        font-size: 14px;
        font-weight: 600;
}
QMessageBox QLabel#qt_msgbox_informativelabel {
        color: @ink2;
        font-size: 13px;
}
QMessageBox QTextEdit {
        font-family: "JetBrains Mono", Consolas, monospace;
        font-size: 12px;
}
*[role="mono"] {
        font-family: "JetBrains Mono", Consolas, monospace;
        font-size: 12px;
}
QPlainTextEdit[role="mono"], QTextEdit[role="mono"] {
        background: @bg;
}
glabels--ReportBugDialog QPlainTextEdit:read-only {
        font-family: "JetBrains Mono", Consolas, monospace;
        font-size: 12px;
        background: @bg;
}
QWizard QLabel#qt_wizard_title, QWizard QLabel[role="wizardTitle"] {
        color: @ink;
        font-size: 20px;
        font-weight: 700;
}
*[role="wizardPage"] {
        background: @panel;
        border: none;
}
)QSS"
        };


        QString rgbaCss( const QColor& color )
        {
                return QStringLiteral( "rgba(%1, %2, %3, %4%)" )
                        .arg( color.red() ).arg( color.green() ).arg( color.blue() )
                        .arg( qRound( color.alphaF() * 100 ) );
        }


        QColor mix( const QColor& a, const QColor& b, double t )
        {
                return QColor::fromRgbF( a.redF()   * ( 1 - t ) + b.redF()   * t,
                                         a.greenF() * ( 1 - t ) + b.greenF() * t,
                                         a.blueF()  * ( 1 - t ) + b.blueF()  * t );
        }


        QColor alpha( QColor color, double a )
        {
                color.setAlphaF( a );
                return color;
        }


        QString gradient( const QColor& a, const QColor& b, const QColor& c )
        {
                // CSS 135deg: from the top-left corner to the bottom-right corner.
                return QStringLiteral( "qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 %1, stop:0.5 %2, stop:1 %3)" )
                        .arg( a.name(), b.name(), c.name() );
        }
}


namespace glabels::theme
{

        ///
        /// Colour as a stylesheet value
        ///
        QString ThemeStyle::cssColor( const QColor& color )
        {
                if ( color.alpha() == 255 )
                {
                        return color.name( QColor::HexRgb );
                }
                return rgbaCss( color );
        }


        ///
        /// Raw template
        ///
        QString ThemeStyle::styleTemplate()
        {
                QString text;
                for ( const char* part : styleTemplateParts )
                {
                        text += QString::fromUtf8( part );
                }
                return text;
        }


        ///
        /// Token values
        ///
        QHash<QString, QString> ThemeStyle::tokens( const Palette& t, Mode mode )
        {
                QHash<QString, QString> v;

                // Colour tokens (SPEC.md 4.1)
                v["bg"]      = cssColor( t.bg );
                v["chrome"]  = cssColor( t.chrome );
                v["ribbon"]  = cssColor( t.ribbon );
                v["work"]    = cssColor( t.work );
                v["panel"]   = cssColor( t.panel );
                v["line"]    = cssColor( t.line );
                v["line2"]   = cssColor( t.line2 );
                v["ink"]     = cssColor( t.ink );
                v["ink2"]    = cssColor( t.ink2 );
                v["ink3"]    = cssColor( t.ink3 );
                v["accent"]  = cssColor( t.accent );
                v["tint"]    = cssColor( t.tint );
                v["hover"]   = cssColor( t.hover );
                v["pressed"] = cssColor( t.pressed );
                v["ctx"]     = cssColor( t.ctx );
                v["ctxLine"] = cssColor( t.ctxLine );
                v["ok"]      = cssColor( t.ok );
                v["okTint"]  = cssColor( t.okTint );
                v["warn"]    = cssColor( t.warn );
                v["gradA"]   = cssColor( t.gradA );
                v["gradB"]   = cssColor( t.gradB );
                v["gradC"]   = cssColor( t.gradC );
                v["onGrad"]  = cssColor( t.onGrad );

                // Derived values
                const bool dark = ( mode == Mode::Dark );
                const QColor towards = dark ? QColor( Qt::white ) : QColor( Qt::black );
                v["grad"]          = gradient( t.gradA, t.gradB, t.gradC );
                v["gradHover"]     = gradient( mix( t.gradA, towards, 0.10 ), mix( t.gradB, towards, 0.10 ), mix( t.gradC, towards, 0.10 ) );
                v["accentHover"]   = cssColor( mix( t.accent, towards, 0.12 ) );
                v["accentPressed"] = cssColor( mix( t.accent, towards, 0.22 ) );
                v["focusOnAccent"] = cssColor( t.ink );
                v["field"]         = cssColor( t.bg );
                v["warnTint"]      = cssColor( alpha( t.warn, 0.14 ) );

                // Metrics (SPEC.md 4.3)
                auto px = []( int n ) { return QString::number( n ) + QStringLiteral( "px" ); };
                v["mTabStrip"]  = px( Metrics::tabStrip );
                v["mTabInner"]  = px( Metrics::tab - 3 );
                v["mLargeW"]    = px( Metrics::largeW );
                v["mLargeH"]    = px( Metrics::largeH );
                v["mSmallH"]    = px( Metrics::smallH );
                v["mIconBtn"]   = px( Metrics::iconBtn );
                v["mField"]     = px( Metrics::field );
                v["mGroupName"] = px( Metrics::groupName );
                v["mGroupPad"]  = px( Metrics::groupPad );
                v["cardHeader"] = px( Decorator::cardHeader );
                v["cardTop"]    = px( Decorator::cardHeader + 2 );
                v["cardTitleTop"] = px( 11 );

                // Images (recoloured icon files)
                auto file = []( const char* name, const QColor& color ) {
                        return ThemeIcons::file( QString::fromLatin1( name ), color );
                };
                const QColor ink40 = alpha( t.ink, 0.4 );
                v["imgChevronDown"]         = file( "chevron-down", t.ink2 );
                v["imgChevronDownDisabled"] = file( "chevron-down", ink40 );
                v["imgChevronUp"]           = file( "chevron-up", t.ink2 );
                v["imgChevronUpDisabled"]   = file( "chevron-up", ink40 );
                v["imgChevronRight"]        = file( "chevron-right", t.ink3 );
                v["imgCheck"]               = file( "check", t.onGrad );
                v["imgCheckAccent"]         = file( "check", t.accent );
                v["imgMinus"]               = file( "minus", t.onGrad );
                v["imgRadioOff"]            = ThemeIcons::radioFile( t.line2, t.panel, Qt::transparent );
                v["imgRadioOffHover"]       = ThemeIcons::radioFile( t.accent, t.panel, Qt::transparent );
                v["imgRadioOn"]             = ThemeIcons::radioFile( t.accent, t.panel, t.accent );
                v["imgRadioOffDisabled"]    = ThemeIcons::radioFile( t.line, t.chrome, Qt::transparent );
                v["imgRadioOnDisabled"]     = ThemeIcons::radioFile( alpha( t.accent, 0.4 ), t.chrome, alpha( t.accent, 0.4 ) );

                return v;
        }


        ///
        /// Expand tokens
        ///
        QString ThemeStyle::expand( const QString& text,
                                    const QHash<QString, QString>& tokens,
                                    QStringList* unknown )
        {
                static const QRegularExpression tokenRe( QStringLiteral( "@([A-Za-z][A-Za-z0-9]*)(?:/(\\d{1,3}))?" ) );

                QString result;
                result.reserve( text.size() + text.size() / 2 );

                qsizetype last = 0;
                auto it = tokenRe.globalMatch( text );
                while ( it.hasNext() )
                {
                        auto m = it.next();
                        result += QStringView( text ).mid( last, m.capturedStart() - last );
                        last = m.capturedEnd();

                        const QString name = m.captured( 1 );
                        auto value = tokens.constFind( name );
                        if ( value == tokens.constEnd() )
                        {
                                if ( unknown )
                                {
                                        unknown->append( name );
                                }
                                result += m.captured( 0 );
                                continue;
                        }

                        if ( m.capturedLength( 2 ) > 0 )
                        {
                                QColor color( *value );
                                if ( color.isValid() )
                                {
                                        color.setAlphaF( qBound( 0, m.captured( 2 ).toInt(), 100 ) / 100.0 );
                                        result += rgbaCss( color );
                                        continue;
                                }
                        }
                        result += *value;
                }
                result += QStringView( text ).mid( last );

                return result;
        }


        ///
        /// Generate the stylesheet
        ///
        QString ThemeStyle::generate( const Palette& palette, Mode mode )
        {
                QStringList unknown;
                QString css = expand( styleTemplate(), tokens( palette, mode ), &unknown );
                if ( !unknown.isEmpty() )
                {
                        qWarning() << "ThemeStyle: unknown tokens" << unknown;
                }
                return css;
        }

}
