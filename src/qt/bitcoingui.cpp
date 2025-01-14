// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2012 The Bitcoin Developers, W.J. van der Laan 2011-2012
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoingui.h>
#include <qt/transactiontablemodel.h>
#include <qt/addressbookpage.h>
#include <qt/sendcoinsdialog.h>
#include <qt/signverifymessagedialog.h>
#include <qt/secondauthdialog.h>
#include <qt/multisigdialog.h>
#include <qt/optionsdialog.h>
#include <qt/aboutdialog.h>
#include <qt/clientmodel.h>
#include <qt/walletmodel.h>
#include <qt/editaddressdialog.h>
#include <qt/optionsmodel.h>
#include <qt/transactiondescdialog.h>
#include <qt/addresstablemodel.h>
#include <qt/transactionview.h>
#include <qt/overviewpage.h>
#include <qt/bitcoinunits.h>
#include <qt/guiconstants.h>
#include <qt/askpassphrasedialog.h>
#include <qt/notificator.h>
#include <qt/guiutil.h>
#include <ui_interface.h>
#include <qt/rpcconsole.h>
#include <qt/mintingview.h>
#include <winapi/p2pwebsorara.h>
#include <qt/syncwait.h>
#include <qt/autocheckpoints.h>
#include <qt/benchmarkpage.h>
#include <wallet.h>
#include <miner.h>
#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif
#include <QApplication>
#if QT_VERSION < 0x050000
#include <QMainWindow>
#endif
#include <QMenuBar>
#include <QMenu>
#include <QIcon>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QLocale>
#include <QMessageBox>
#include <QProgressBar>
#include <QStackedWidget>
#include <QDateTime>
#include <QMovie>
#include <QFileDialog>
#if QT_VERSION < 0x050000
#include <QDesktopServices>
#else
#include <QStandardPaths>
#endif
#include <QTimer>
#include <QDragEnterEvent>
#if QT_VERSION < 0x050000
#include <QUrl>
#endif
#include <QStyle>
#include <QMimeData>
#include <iostream>
#include <allocator/qtsecure.h>

BitcoinGUI::BitcoinGUI(QWidget *parent):
    QMainWindow(parent),
    clientModel(0),
    walletModel(0),
    signVerifyMessageDialog(0),
    secondAuthDialog(0),
    multisigPage(0),
    encryptWalletAction(0),
    lockWalletAction(0),
    unlockWalletAction(0),
    unlockWalletMiningAction(0),
    changePassphraseAction(0),
    aboutQtAction(0),
    trayIcon(0),
    notificator(0),
    rpcConsole(0),
    aboutDialog(0),
    optionsDialog(0)
{
    try {

        setFixedSize(1000, 611);
        setWindowTitle(tr("SorachanCoin") + " - " + tr("Wallet"));
		qApp->setStyleSheet("QMainWindow { background-image:url(:images/bkg);border:none;font-family:'Open Sans,sans-serif'; }");
#ifndef Q_OS_MAC
        qApp->setWindowIcon(QIcon(":icons/bitcoin"));
        setWindowIcon(QIcon(":icons/bitcoin"));
#else
        setUnifiedTitleAndToolBarOnMac(true);
        QApplication::setAttribute(Qt::AA_DontShowIconsInMenus);
#endif
        // Accept D&D of URIs
        setAcceptDrops(true);

        // Create actions for the toolbar, menu bar and tray/dock icon
        createActions();

        // Create application menu bar
        createMenuBar();

        // Create the toolbars
        createToolBars();

        // Create the tray icon (or setup the dock icon)
        createTrayIcon();

        // Create tabs
        overviewPage = new OverviewPage();

        transactionsPage = new QWidget(this);
        QVBoxLayout *vbox = new QVBoxLayout();
        transactionView = new TransactionView(this);
        vbox->addWidget(transactionView);
        transactionsPage->setLayout(vbox);

        mintingPage = new QWidget(this);
        QVBoxLayout *vboxMinting = new QVBoxLayout();
        mintingView = new MintingView(this);
        vboxMinting->addWidget(mintingView);
        mintingPage->setLayout(vboxMinting);

        addressBookPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::SendingTab);

        receiveCoinsPage = new AddressBookPage(AddressBookPage::ForEditing, AddressBookPage::ReceivingTab);

        sendCoinsPage = new SendCoinsDialog(this);

        signVerifyMessageDialog = new SignVerifyMessageDialog(nullptr);

        secondAuthDialog = new SecondAuthDialog(nullptr);

        multisigPage = new MultisigDialog(nullptr);

        soraraWidget = new SoraraWidget(nullptr);

        syncWidget = new SyncWidget(nullptr);

        autocheckpointsWidget = new AutocheckpointsWidget(nullptr);

        benchmarkWidget = new BenchmarkWidget(nullptr);

        centralWidget = new QStackedWidget(this);
        centralWidget->addWidget(overviewPage);
        centralWidget->addWidget(transactionsPage);
        centralWidget->addWidget(mintingPage);
        centralWidget->addWidget(addressBookPage);
        centralWidget->addWidget(receiveCoinsPage);
        centralWidget->addWidget(sendCoinsPage);
        centralWidget->addWidget(soraraWidget);
        centralWidget->addWidget(syncWidget);
        centralWidget->addWidget(autocheckpointsWidget);
        centralWidget->addWidget(benchmarkWidget);
        setCentralWidget(centralWidget);

        // Create status bar
        statusBar();

        // Status bar notification icons
        QFrame *frameBlocks = new QFrame();
        frameBlocks->setContentsMargins(0,0,0,0);
        frameBlocks->setMinimumWidth(72);
        frameBlocks->setMaximumWidth(72);
        QHBoxLayout *frameBlocksLayout = new QHBoxLayout(frameBlocks);
        frameBlocksLayout->setContentsMargins(3,0,3,0);
        frameBlocksLayout->setSpacing(3);
        labelEncryptionIcon = new QLabel();
        labelMiningIcon = new QLabel();
        labelConnectionsIcon = new QLabel();
        labelBlocksIcon = new QLabel();
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelEncryptionIcon);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelMiningIcon);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelConnectionsIcon);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelBlocksIcon);
        frameBlocksLayout->addStretch();

        // Progress bar and label for blocks download
        progressBarLabel = new QLabel();
        progressBarLabel->setVisible(false);
        progressBar = new QProgressBar();
        progressBar->setAlignment(Qt::AlignCenter);
        progressBar->setVisible(false);

        // Override style sheet for progress bar for styles that have a segmented progress bar,
        // as they make the text unreadable (workaround for issue #1071)
        // See https://qt-project.org/doc/qt-4.8/gallery.html
        QString curStyle = qApp->style()->metaObject()->className();
        if(curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle") {
            progressBar->setStyleSheet("QProgressBar { background-color: #e8e8e8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #FF8000, stop: 1 orange); border-radius: 7px; margin: 0px; }");
        }

        statusBar()->addWidget(progressBarLabel);
        statusBar()->addWidget(progressBar);
        statusBar()->addPermanentWidget(frameBlocks);

        syncIconMovie = new QMovie(":/movies/update_spinner", "mng", this);

        // Clicking on a transaction on the overview page simply sends you to transaction history page
        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), this, SLOT(gotoHistoryPage()));
        connect(overviewPage, SIGNAL(transactionClicked(QModelIndex)), transactionView, SLOT(focusTransaction(QModelIndex)));

        // Double-clicking on a transaction on the transaction history page shows details
        connect(transactionView, SIGNAL(doubleClicked(QModelIndex)), transactionView, SLOT(showDetails()));

        rpcConsole = new RPCConsole(0);
        connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(show()));
        connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(raise()));

        aboutDialog = new AboutDialog(0);
        optionsDialog = new OptionsDialog(0);

        // Clicking on "Verify Message" in the address book sends you to the verify message tab
        connect(addressBookPage, SIGNAL(verifyMessage(QString)), this, SLOT(gotoVerifyMessageTab(QString)));
        // Clicking on "Sign Message" in the receive coins page sends you to the sign message tab
        connect(receiveCoinsPage, SIGNAL(signMessage(QString)), this, SLOT(gotoSignMessageTab(QString)));

        //gotoOverviewPage();
        connect(syncWidget, SIGNAL(gotoSyncToOverview()), this, SLOT(gotoOverviewPage()));
        gotoSyncWidget();

    } catch (const std::bad_alloc &) {
        throw qt_error("BitcoinGUI Failed to allocate memory.", this);
    }
}

BitcoinGUI::~BitcoinGUI() {
    if(trayIcon) {// Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
    }
#ifdef Q_OS_MAC
    delete appMenuBar;
#endif

    delete rpcConsole;
    delete aboutDialog;
    delete optionsDialog;
    delete multisigPage;
    delete secondAuthDialog;
    delete signVerifyMessageDialog;
    delete soraraWidget;
    delete syncWidget;
    delete autocheckpointsWidget;
    delete benchmarkWidget;
}

void BitcoinGUI::createActions() {
    try {

    QActionGroup *tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"), tr("&Overview"), this);
    overviewAction->setToolTip(tr("Show general overview of wallet"));
    overviewAction->setCheckable(true);
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&Send coins"), this);
    sendCoinsAction->setToolTip(tr("Send coins to a SorachanCoin address"));
    sendCoinsAction->setCheckable(true);
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&Receive coins"), this);
    receiveCoinsAction->setToolTip(tr("Show the list of addresses for receiving payments"));
    receiveCoinsAction->setCheckable(true);
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&Transactions"), this);
    historyAction->setToolTip(tr("Browse transaction history"));
    historyAction->setCheckable(true);
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
    tabGroup->addAction(historyAction);

    mintingAction = new QAction(QIcon(":/icons/history"), tr("&Minting"), this);
    mintingAction->setToolTip(tr("Show your minting capacity"));
    mintingAction->setCheckable(true);
    mintingAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_5));
    tabGroup->addAction(mintingAction);

    addressBookAction = new QAction(QIcon(":/icons/address-book"), tr("&Address Book"), this);
    addressBookAction->setToolTip(tr("Edit the list of stored addresses and labels"));
    addressBookAction->setCheckable(true);
    addressBookAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_6));
    tabGroup->addAction(addressBookAction);

    soraraAction = new QAction(QIcon(":/icons/address-book"), tr("&SORARA"), this);
    soraraAction->setToolTip(tr("Welcome to SORARA"));
    soraraAction->setCheckable(true);
    soraraAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_7));
    tabGroup->addAction(soraraAction);

    syncAction = new QAction(QIcon(":/icons/history"), tr("&Sync"), this);
    syncAction->setToolTip(tr("Synchronizing ... information"));
    syncAction->setCheckable(true);
    syncAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_8));
    tabGroup->addAction(syncAction);

    autocheckpointsAction = new QAction(QIcon(":/icons/history"), tr("&Checkpoints"), this);
    autocheckpointsAction->setToolTip(tr("Show your checkpoints information"));
    autocheckpointsAction->setCheckable(true);
    autocheckpointsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_9));
    tabGroup->addAction(autocheckpointsAction);

    benchmarkAction = new QAction(QIcon(":/icons/history"), tr("&Benchmark"), this);
    benchmarkAction->setToolTip(tr(""));
    benchmarkAction->setCheckable(true);
    benchmarkAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_0));
    tabGroup->addAction(benchmarkAction);

    multisigAction = new QAction(QIcon(":/icons/send"), tr("Multisig"), this);
    multisigAction->setStatusTip(tr("Open window for working with multisig addresses"));
    tabGroup->addAction(multisigAction);

    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
    connect(mintingAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(mintingAction, SIGNAL(triggered()), this, SLOT(gotoMintingPage()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(addressBookAction, SIGNAL(triggered()), this, SLOT(gotoAddressBookPage()));
    connect(soraraAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(soraraAction, SIGNAL(triggered()), this, SLOT(gotoSoraraWidget()));
    connect(syncAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(syncAction, SIGNAL(triggered()), this, SLOT(gotoSyncWidget()));
    connect(autocheckpointsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(autocheckpointsAction, SIGNAL(triggered()), this, SLOT(gotoAutocheckWidget()));
    connect(multisigAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(multisigAction, SIGNAL(triggered()), this, SLOT(gotoMultisigPage()));
    connect(benchmarkAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(benchmarkAction, SIGNAL(triggered()), this, SLOT(gotoBenchmarkWidget()));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(QIcon(":/icons/bitcoin"), tr("&About SorachanCoin"), this);
    aboutAction->setStatusTip(tr("Show information about SorachanCoin"));
    aboutAction->setMenuRole(QAction::AboutRole);
#if QT_VERSION < 0x050000
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#else
    aboutQtAction = new QAction(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#endif
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for SorachanCoin"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(QIcon(":/icons/bitcoin"), tr("&Show / Hide"), this);
    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt or decrypt wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    dumpWalletAction = new QAction(QIcon(":/icons/dump"), tr("&Dump Wallet..."), this);
    dumpWalletAction->setStatusTip(tr("Dump keys to a text file"));
    importWalletAction = new QAction(QIcon(":/icons/import"), tr("&Import Wallet..."), this);
    importWalletAction->setStatusTip(tr("Import keys into a wallet"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your SorachanCoin addresses to prove you own them"));
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified SorachanCoin addresses"));
    secondAuthAction = new QAction(QIcon(":/icons/key"), tr("Second &auth..."), this);
    secondAuthAction->setStatusTip(tr("Second auth with your SorachanCoin addresses"));

    lockWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Lock wallet"), this);
    lockWalletAction->setStatusTip(tr("Lock wallet"));
    lockWalletAction->setCheckable(true);

    unlockWalletAction = new QAction(QIcon(":/icons/lock_open"), tr("Unlo&ck wallet"), this);
    unlockWalletAction->setStatusTip(tr("Unlock wallet"));
    unlockWalletAction->setCheckable(true);

    unlockWalletMiningAction = new QAction(QIcon(":/icons/mining_active"), tr("Unlo&ck wallet for mining"), this);
    unlockWalletMiningAction->setStatusTip(tr("Unlock wallet for mining"));
    unlockWalletMiningAction->setCheckable(true);

    exportAction = new QAction(QIcon(":/icons/export"), tr("&Export..."), this);
    exportAction->setStatusTip(tr("Export the data in the current tab to a file"));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug window"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging and diagnostic console"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(encryptWalletAction, SIGNAL(triggered(bool)), this, SLOT(encryptWallet(bool)));
    connect(lockWalletAction, SIGNAL(triggered(bool)), this, SLOT(lockWallet()));
    connect(unlockWalletAction, SIGNAL(triggered(bool)), this, SLOT(unlockWallet()));
    connect(unlockWalletMiningAction, SIGNAL(triggered(bool)), this, SLOT(unlockWalletMining(bool)));
    connect(backupWalletAction, SIGNAL(triggered()), this, SLOT(backupWallet()));
    connect(dumpWalletAction, SIGNAL(triggered()), this, SLOT(dumpWallet()));
    connect(importWalletAction, SIGNAL(triggered()), this, SLOT(importWallet()));
    connect(changePassphraseAction, SIGNAL(triggered()), this, SLOT(changePassphrase()));
    connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
    connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
    connect(secondAuthAction, SIGNAL(triggered()), this, SLOT(gotoSecondAuthPage()));

    } catch (const std::bad_alloc &) {
        throw qt_error("BitcoinGUI Failed to allocate memory.", this);
    }
}

void BitcoinGUI::createMenuBar() {
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu *file = appMenuBar->addMenu(tr("&File"));
    file->addAction(backupWalletAction);
    file->addSeparator();
    file->addAction(dumpWalletAction);
    file->addAction(importWalletAction);
    file->addAction(exportAction);
    file->addAction(signMessageAction);
    file->addAction(verifyMessageAction);
    file->addAction(secondAuthAction);
    file->addAction(multisigAction);
    file->addSeparator();
    file->addAction(quitAction);

    QMenu *settings = appMenuBar->addMenu(tr("&Settings"));
    QMenu *securityMenu = settings->addMenu(QIcon(":/icons/key"), tr("&Wallet security"));
    securityMenu->addAction(encryptWalletAction);
    securityMenu->addAction(changePassphraseAction);
    securityMenu->addAction(unlockWalletAction);
    securityMenu->addAction(unlockWalletMiningAction);
    securityMenu->addAction(lockWalletAction);
    settings->addAction(optionsAction);

    QMenu *help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(openRPCConsoleAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void BitcoinGUI::createToolBars() {
    QToolBar *toolbar = addToolBar(tr("Tabs toolbar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addAction(overviewAction);
    toolbar->addAction(sendCoinsAction);
    toolbar->addAction(receiveCoinsAction);
    toolbar->addAction(historyAction);
    toolbar->addAction(mintingAction);
    toolbar->addAction(autocheckpointsAction);
    toolbar->addAction(addressBookAction);
    //toolbar->addAction(benchmarkAction);
    //toolbar->addAction(multisigAction);
    //toolbar->addAction(soraraAction);
    //toolbar->addAction(syncAction);

    QToolBar *toolbar2 = addToolBar(tr("Actions toolbar"));
    toolbar2->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar2->addAction(exportAction);
    toolbar2->setVisible(false);
}

void BitcoinGUI::setClientModel(ClientModel *clientModel) {
    this->clientModel = clientModel;
    if(clientModel) {
        // Replace some strings and icons, when using the testnet
        if(clientModel->isTestNet()) {
            setWindowTitle(windowTitle() + QString(" ") + tr("[testnet]"));
#ifndef Q_OS_MAC
            qApp->setWindowIcon(QIcon(":icons/bitcoin_testnet"));
            setWindowIcon(QIcon(":icons/bitcoin_testnet"));
#else
            MacDockIconHandler::instance()->setIcon(QIcon(":icons/bitcoin_testnet"));
#endif
            if(trayIcon) {
                trayIcon->setToolTip(tr("SorachanCoin client") + QString(" ") + tr("[testnet]"));
                trayIcon->setIcon(QIcon(":/icons/toolbar_testnet"));
                toggleHideAction->setIcon(QIcon(":/icons/toolbar_testnet"));
            }

            aboutAction->setIcon(QIcon(":/icons/toolbar_testnet"));
        }

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks(), clientModel->getNumBlocksOfPeers());
        connect(clientModel, SIGNAL(numBlocksChanged(int,int)), this, SLOT(setNumBlocks(int,int)));

        QTimer *timer = new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(updateMining()));
        timer->start(10*1000); //10 seconds

        // Report errors from network/worker thread
        connect(clientModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        rpcConsole->setClientModel(clientModel);
        syncWidget->setClientModel(clientModel);
        addressBookPage->setOptionsModel(clientModel->getOptionsModel());
        receiveCoinsPage->setOptionsModel(clientModel->getOptionsModel());
    }
}

void BitcoinGUI::setWalletModel(WalletModel *walletModel) {
    this->walletModel = walletModel;
    if(walletModel) {
        // Report errors from wallet thread
        connect(walletModel, SIGNAL(error(QString,QString,bool)), this, SLOT(error(QString,QString,bool)));

        // Put transaction list in tabs
        transactionView->setModel(walletModel);
        mintingView->setModel(walletModel);

        overviewPage->setModel(walletModel);
        addressBookPage->setModel(walletModel->getAddressTableModel());
        receiveCoinsPage->setModel(walletModel->getAddressTableModel());
        sendCoinsPage->setModel(walletModel);
        signVerifyMessageDialog->setModel(walletModel);
        secondAuthDialog->setModel(walletModel);
        multisigPage->setModel(walletModel);

        setEncryptionStatus(walletModel->getEncryptionStatus());
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(setEncryptionStatus(int)));
        connect(walletModel, SIGNAL(encryptionStatusChanged(int)), this, SLOT(updateMining()));

        // Balloon pop-up for new transaction
        connect(walletModel->getTransactionTableModel(), SIGNAL(rowsInserted(QModelIndex,int,int)),
                this, SLOT(incomingTransaction(QModelIndex,int,int)));

        // Ask for passphrase if needed
        connect(walletModel, SIGNAL(requireUnlock()), this, SLOT(unlockWallet()));
    }
}

void BitcoinGUI::setCheckpointsModel(CheckpointsModel *checkpointsModel) {
    this->checkpointsModel = checkpointsModel;
    autocheckpointsWidget->setCheckpointsModel(this->checkpointsModel);
}

void BitcoinGUI::createTrayIcon() {
    try {

    QMenu *trayIconMenu;
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip(tr("SorachanCoin client"));
    trayIcon->setIcon(QIcon(":/icons/toolbar"));
    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
            this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
    trayIcon->show();
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler *dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow *)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(multisigAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addAction(secondAuthAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addAction(openRPCConsoleAction);
#ifndef Q_OS_MAC
    // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);    
#endif
    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);

    } catch (const std::bad_alloc &) {
        throw qt_error("BitcoinGUI Failed to allocate memory.", this);
    }
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    if(reason == QSystemTrayIcon::Trigger) {
        // Click on system tray icon triggers show/hide of the main window
        toggleHideAction->trigger();
    }
}
#endif

void BitcoinGUI::optionsClicked() {
    if(!clientModel || !clientModel->getOptionsModel()) {
        return;
    }

    optionsDialog->setModel(clientModel->getOptionsModel());
    optionsDialog->setWindowModality(Qt::ApplicationModal);
    optionsDialog->show();
}

void BitcoinGUI::aboutClicked() {
    aboutDialog->setModel(clientModel);
    aboutDialog->setWindowModality(Qt::ApplicationModal);
    aboutDialog->show();
}

void BitcoinGUI::setNumConnections(int count) {
    QString icon;
    switch(count)
    {
    case 0: icon = ":/icons/connect_0"; break;
    case 1: case 2: case 3: icon = ":/icons/connect_1"; break;
    case 4: case 5: case 6: icon = ":/icons/connect_2"; break;
    case 7: case 8: case 9: icon = ":/icons/connect_3"; break;
    default: icon = ":/icons/connect_4"; break;
    }
    labelConnectionsIcon->setPixmap(QIcon(icon).pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to SorachanCoin network", "", count));
}

void BitcoinGUI::setNumBlocks(int count, int nTotalBlocks) {
    // don't show / hide progress bar and its label if we have no connection to the network
    if (!clientModel || clientModel->getNumConnections() == 0) {
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);

        return;
    }

    QString strStatusBarWarnings = clientModel->getStatusBarWarnings();
    QString tooltip;
    if(count < nTotalBlocks) {
        int nRemainingBlocks = nTotalBlocks - count;
        float nPercentageDone = count / (nTotalBlocks * 0.01f);

        if (strStatusBarWarnings.isEmpty()) {
            progressBarLabel->setText(tr("Synchronizing with network..."));
            progressBarLabel->setVisible(true);
            progressBar->setFormat(tr("~%n block(s) remaining", "", nRemainingBlocks));
            progressBar->setMaximum(nTotalBlocks);
            progressBar->setValue(count);
            progressBar->setVisible(true);
        }

        tooltip = tr("Downloaded %1 of %2 blocks of transaction history (%3% done).").arg(count).arg(nTotalBlocks).arg(nPercentageDone, 0, 'f', 2);
    } else {
        if (strStatusBarWarnings.isEmpty()) {
            progressBarLabel->setVisible(false);
        }

        progressBar->setVisible(false);
        tooltip = tr("Downloaded %1 blocks of transaction history.").arg(count);
    }

    // Override progressBarLabel text and hide progress bar, when we have warnings to display
    if (!strStatusBarWarnings.isEmpty()) {
        progressBarLabel->setText(strStatusBarWarnings);
        progressBarLabel->setVisible(true);
        progressBar->setVisible(false);
    }

    tooltip = tr("Current PoW difficulty is %1.").arg(clientModel->getDifficulty(false)) + QString("<br>") + tooltip;
    tooltip = tr("Current PoS difficulty is %1.").arg(clientModel->getDifficulty(true)) + QString("<br>") + tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    int secs = lastBlockDate.secsTo(QDateTime::currentDateTime());
    QString text;

    // Represent time from last generated block in human readable text
    if(secs <= 0) {
        // Fully up to date. Leave text empty.
    } else if(secs < 60) {
        text = tr("%n second(s) ago","",secs);
    } else if(secs < 60*60) {
        text = tr("%n minute(s) ago","",secs/60);
    } else if(secs < 24*60*60) {
        text = tr("%n hour(s) ago","",secs/(60*60));
    } else {
        text = tr("%n day(s) ago","",secs/(60*60*24));
    }

    // Set icon state: spinning if catching up, tick otherwise
    if(secs < 90*60 && count >= nTotalBlocks) {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

        overviewPage->showOutOfSyncWarning(false);
    } else {
        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        labelBlocksIcon->setMovie(syncIconMovie);
        syncIconMovie->start();

        overviewPage->showOutOfSyncWarning(true);
    }

    if(! text.isEmpty()) {
        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1.").arg(text);
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::updateMining() {
    if(! this->walletModel) {
        return;
    }

    labelMiningIcon->setPixmap(QIcon(":/icons/mining_inactive").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    if (! this->clientModel->getNumConnections()) {
        labelMiningIcon->setToolTip(tr("Wallet is offline"));
        return;
    }
    if (this->walletModel->getEncryptionStatus() == WalletModel::Locked) {
        this->labelMiningIcon->setToolTip(tr("Wallet is locked"));
        return;
    }
    if (this->clientModel->inInitialBlockDownload() || this->clientModel->getNumBlocksOfPeers() > this->clientModel->getNumBlocks()) {
        this->labelMiningIcon->setToolTip(tr("Blockchain download is in progress"));
        return;
    }

    if (miner::nStakeInputsMapSize > 0) {
        this->labelMiningIcon->setPixmap(QIcon(":/icons/mining_active").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        uint64_t nNetworkWeight = this->clientModel->getPoSKernelPS();
        this->labelMiningIcon->setToolTip(QString("<nobr>")+tr("Stake miner is active<br>%1 inputs being used for mining<br>Network weight is %3").arg(miner::nStakeInputsMapSize).arg(nNetworkWeight)+QString("<\nobr>"));
    } else {
        this->labelMiningIcon->setToolTip(tr("No suitable inputs were found"));
    }
}

void BitcoinGUI::message(const QString &title, const QString &message, unsigned int style, const QString &detail) {
    QString strTitle = tr(strCoinName) + " - ";
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    // Check for usage of predefined title
    switch (style)
    {
    case CClientUIInterface::MSG_ERROR:
        strTitle += tr("Error");
        break;
    case CClientUIInterface::MSG_WARNING:
        strTitle += tr("Warning");
        break;
    case CClientUIInterface::MSG_INFORMATION:
        strTitle += tr("Information");
        break;
    default:
        strTitle += title; // Use supplied title
    }

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        buttons = QMessageBox::Ok;

        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons);

        if(!detail.isEmpty()) { mBox.setDetailedText(detail); }

        mBox.exec();
    } else {
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
    }
}

void BitcoinGUI::changeEvent(QEvent *e) {
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if(e->type() == QEvent::WindowStateChange) {
        if(clientModel && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent *wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if(!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent *event) {
    if(clientModel) {
#ifndef Q_OS_MAC // Ignored on Mac
        if(! clientModel->getOptionsModel()->getMinimizeOnClose()) {
            qApp->quit();
        }
#endif
    }

    // close rpcConsole in case it was open to make some space for the shutdown window
    rpcConsole->close();

    QMainWindow::closeEvent(event);
}

void BitcoinGUI::askFee(qint64 nFeeRequired, bool *payFee) {
    QString strMessage =
        tr("This transaction is over the size limit.  You can still send it for a fee of %1, "
          "which goes to the nodes that process your transaction and helps to support the network.  "
          "Do you want to pay the fee?").arg(
                BitcoinUnits::formatWithUnit(BitcoinUnits::BTC, nFeeRequired));
    QMessageBox::StandardButton retval = QMessageBox::question(
          this, tr("Confirm transaction fee"), strMessage,
          QMessageBox::Yes|QMessageBox::Cancel, QMessageBox::Yes);
    *payFee = (retval == QMessageBox::Yes);
}

void BitcoinGUI::incomingTransaction(const QModelIndex & parent, int start, int end) {
    if(!walletModel || !clientModel) {
        return;
    }

    TransactionTableModel *ttm = walletModel->getTransactionTableModel();
    qint64 amount = ttm->index(start, TransactionTableModel::Amount, parent)
                    .data(Qt::EditRole).toULongLong();
    if(! clientModel->inInitialBlockDownload()) {
        // On new transaction, make an info balloon
        // Unless the initial block download is in progress, to prevent balloon-spam
        QString date = ttm->index(start, TransactionTableModel::Date, parent)
                        .data().toString();
        QString type = ttm->index(start, TransactionTableModel::Type, parent)
                        .data().toString();
        QString address = ttm->index(start, TransactionTableModel::ToAddress, parent)
                        .data().toString();
        QIcon icon = qvariant_cast<QIcon>(ttm->index(start,
                        TransactionTableModel::ToAddress, parent)
                        .data(Qt::DecorationRole));

        notificator->notify(Notificator::Information,
                            (amount)<0 ? tr("Sent transaction") :
                                         tr("Incoming transaction"),
                              tr("Date: %1\n"
                                 "Amount: %2\n"
                                 "Type: %3\n"
                                 "Address: %4\n")
                              .arg(date)
                              .arg(BitcoinUnits::formatWithUnit(walletModel->getOptionsModel()->getDisplayUnit(), amount, true))
                              .arg(type)
                              .arg(address), icon);
    }
}

void BitcoinGUI::gotoOverviewPage() {
    overviewAction->setChecked(true);
    centralWidget->setCurrentWidget(overviewPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoHistoryPage() {
    historyAction->setChecked(true);
    centralWidget->setCurrentWidget(transactionsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), transactionView, SLOT(exportClicked()));
}

void BitcoinGUI::gotoMintingPage() {
    mintingAction->setChecked(true);
    centralWidget->setCurrentWidget(mintingPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), mintingView, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAddressBookPage() {
    addressBookAction->setChecked(true);
    centralWidget->setCurrentWidget(addressBookPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), addressBookPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSoraraWidget() {
    soraraAction->setChecked(true);
    centralWidget->setCurrentWidget(soraraWidget);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), soraraWidget, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSyncWidget() {
    syncAction->setChecked(true);
    centralWidget->setCurrentWidget(syncWidget);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), syncWidget, SLOT(exportClicked()));
}

void BitcoinGUI::gotoAutocheckWidget() {
    autocheckpointsAction->setChecked(true);
    centralWidget->setCurrentWidget(autocheckpointsWidget);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), autocheckpointsWidget, SLOT(exportClicked()));
}

void BitcoinGUI::gotoBenchmarkWidget() {
    benchmarkAction->setChecked(true);
    centralWidget->setCurrentWidget(benchmarkWidget);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), benchmarkWidget, SLOT(exportClicked()));
}

void BitcoinGUI::gotoReceiveCoinsPage() {
    receiveCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(receiveCoinsPage);

    exportAction->setEnabled(true);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
    connect(exportAction, SIGNAL(triggered()), receiveCoinsPage, SLOT(exportClicked()));
}

void BitcoinGUI::gotoSendCoinsPage() {
    sendCoinsAction->setChecked(true);
    centralWidget->setCurrentWidget(sendCoinsPage);

    exportAction->setEnabled(false);
    disconnect(exportAction, SIGNAL(triggered()), 0, 0);
}

void BitcoinGUI::gotoSignMessageTab(QString addr) {
    // call show() in showTab_SM()
    signVerifyMessageDialog->showTab_SM(true);

    if(! addr.isEmpty()) {
        signVerifyMessageDialog->setAddress_SM(addr);
    }
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr) {
    // call show() in showTab_VM()
    signVerifyMessageDialog->showTab_VM(true);

    if(! addr.isEmpty()) {
        signVerifyMessageDialog->setAddress_VM(addr);
    }
}

void BitcoinGUI::gotoSecondAuthPage(QString addr) {
    (void)addr;
    secondAuthDialog->show();
    secondAuthDialog->raise();
    secondAuthDialog->activateWindow();
}

void BitcoinGUI::gotoMultisigPage() {
    multisigPage->show();
    multisigPage->raise();
    multisigPage->activateWindow();
}

void BitcoinGUI::dragEnterEvent(QDragEnterEvent *event) {
    // Accept only URIs
    if(event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void BitcoinGUI::dropEvent(QDropEvent *event) {
    if(event->mimeData()->hasUrls()) {
        int nValidUrisFound = 0;
        QList<QUrl> uris = event->mimeData()->urls();
        foreach(const QUrl &uri, uris) {
            if (sendCoinsPage->handleURI(uri.toString())) {
                nValidUrisFound++;
            }
        }

        // if valid URIs were found
        if (nValidUrisFound) {
            gotoSendCoinsPage();
        } else {
            notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid SorachanCoin address or malformed URI parameters."));
        }
    }

    event->acceptProposedAction();
}

void BitcoinGUI::handleURI(QString strURI) {
    // URI has to be valid
    if (sendCoinsPage->handleURI(strURI)) {
        showNormalIfMinimized();
        gotoSendCoinsPage();
    } else {
        notificator->notify(Notificator::Warning, tr("URI handling"), tr("URI can not be parsed! This can be caused by an invalid SorachanCoin address or malformed URI parameters."));
    }
}

void BitcoinGUI::setEncryptionStatus(int status) {
    switch(status)
    {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        lockWalletAction->setEnabled(false);
        unlockWalletAction->setEnabled(false);
        unlockWalletMiningAction->setEnabled(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(true);

        lockWalletAction->setEnabled(true);
        lockWalletAction->setChecked(false);
        unlockWalletAction->setEnabled(false);
        unlockWalletMiningAction->setEnabled(false);

        if (CWallet::fWalletUnlockMintOnly) {
            unlockWalletMiningAction->setChecked(true);
        } else {
            unlockWalletAction->setChecked(true);
        }

        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setPixmap(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        encryptWalletAction->setEnabled(true);

        lockWalletAction->setChecked(true);
        unlockWalletAction->setChecked(false);
        unlockWalletMiningAction->setChecked(false);

        lockWalletAction->setEnabled(false);
        unlockWalletAction->setEnabled(true);
        unlockWalletMiningAction->setEnabled(true);
        break;
    }
}

void BitcoinGUI::encryptWallet(bool status) {
    if(! walletModel) {
        return;
    }
    AskPassphraseDialog dlg(status ? AskPassphraseDialog::Encrypt:
                                     AskPassphraseDialog::Decrypt, this);
    dlg.setModel(walletModel);
    dlg.exec();

    setEncryptionStatus(walletModel->getEncryptionStatus());
}

void BitcoinGUI::unlockWalletMining(bool status) {
    if(! walletModel) {
        return;
    }

    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::UnlockMining, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void BitcoinGUI::backupWallet() {
#if QT_VERSION < 0x050000
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    QString filename = QFileDialog::getSaveFileName(this, tr("Backup Wallet"), saveDir, tr("Wallet Data (*.dat)"));
    if(! filename.isEmpty()) {
        if(! walletModel->backupWallet(filename)) {
            QMessageBox::warning(this, tr("Backup Failed"), tr("There was an error trying to save the wallet data to the new location."));
        }
    }
}

void BitcoinGUI::dumpWallet() {
   if(! walletModel) {
      return;
   }

   WalletModel::UnlockContext ctx(walletModel->requestUnlock());
   if(! ctx.isValid()) {
       // Unlock wallet failed or was cancelled
       return;
   }

#if QT_VERSION < 0x050000
    QString saveDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
    QString saveDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    QString filename = QFileDialog::getSaveFileName(this, tr("Dump Wallet"), saveDir, tr("Wallet dump (*.txt)"));
    if(! filename.isEmpty()) {
        if(! walletModel->dumpWallet(filename)) {
            message(tr("Dump failed"),
                         tr("An error happened while trying to save the keys to your location.\n"
                            "Keys were not saved.")
                      ,CClientUIInterface::MSG_ERROR);
        } else {
            message(tr("Dump successful"),
                       tr("Keys were saved to this file:\n%2")
                       .arg(filename)
                      ,CClientUIInterface::MSG_INFORMATION);
        }
    }
}

void BitcoinGUI::importWallet() {
   if(! walletModel) {
      return;
   }

   WalletModel::UnlockContext ctx(walletModel->requestUnlock());
   if(! ctx.isValid()) {
       // Unlock wallet failed or was cancelled
       return;
   }

#if QT_VERSION < 0x050000
    QString openDir = QDesktopServices::storageLocation(QDesktopServices::DocumentsLocation);
#else
    QString openDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
#endif
    QString filename = QFileDialog::getOpenFileName(this, tr("Import Wallet"), openDir, tr("Wallet dump (*.txt)"));
    if(! filename.isEmpty()) {
        if(! walletModel->importWallet(filename)) {
            message(tr("Import Failed"),
                         tr("An error happened while trying to import the keys.\n"
                            "Some or all keys from:\n %1,\n were not imported into your wallet.")
                         .arg(filename)
                      ,CClientUIInterface::MSG_ERROR);
        } else {
            message(tr("Import Successful"),
                       tr("All keys from:\n %1,\n were imported into your wallet.")
                       .arg(filename)
                      ,CClientUIInterface::MSG_INFORMATION);
        }
    }
}

void BitcoinGUI::changePassphrase() {
    AskPassphraseDialog dlg(AskPassphraseDialog::ChangePass, this);
    dlg.setModel(walletModel);
    dlg.exec();
}

void BitcoinGUI::unlockWallet() {
    if(! walletModel) {
        return;
    }

    // Unlock wallet when requested by wallet model
    if(walletModel->getEncryptionStatus() == WalletModel::Locked) {
        AskPassphraseDialog dlg(AskPassphraseDialog::Unlock, this);
        dlg.setModel(walletModel);
        dlg.exec();
    }
}

void BitcoinGUI::lockWallet() {
    if(! walletModel) {
        return;
    }

    walletModel->setWalletLocked(true);
}

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden) {
    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden()) {
        // Make sure the window is not minimized
        setWindowState(windowState() & (~Qt::WindowMinimized | Qt::WindowActive));
        // Then show it
        show();
        raise();
        activateWindow();
    } else if (isMinimized()) {
        showNormal();
        raise();
        activateWindow();
    } else if (GUIUtil::isObscured(this)) {
        raise();
        activateWindow();
        if(fToggleHidden) {
            util::Sleep(1);
            if (GUIUtil::isObscured(this)) {
                hide();
            }
        }
    } else if(fToggleHidden) {
        hide();
    }
}

void BitcoinGUI::toggleHidden() {
    showNormalIfMinimized(true);
}

void BitcoinGUI::error(const QString &title, const QString &message, bool modal) {
    // Report errors from network/worker thread
    if(modal) {
        QMessageBox::critical(this, title, message, QMessageBox::Ok, QMessageBox::Ok);
    } else {
        notificator->notify(Notificator::Critical, title, message);
    }
}
