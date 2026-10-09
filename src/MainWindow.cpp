#include "MainWindow.h"
#include "GastoReal.h"
#include "ModeloPresupuesto.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTableView>
#include <QTableWidget>
#include <QTabWidget>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>

namespace {
QString unirSubtitulo(const Cabecera& c) {
    QStringList partes;
    if (!c.programa.isEmpty()) partes << c.programa;
    if (!c.version.isEmpty()) partes << c.version;
    if (!c.tipoNombre.isEmpty()) partes << c.tipoNombre;
    return partes.join(QStringLiteral(", "));
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Visor BC3"));
    resize(1280, 800);

    auto* central = new QWidget(this);
    auto* raiz = new QVBoxLayout(central);
    raiz->setContentsMargins(16, 12, 16, 8);

    titulo_ = new QLabel(QStringLiteral("Presupuesto BC3"));
    QFont f = titulo_->font();
    f.setPointSize(16);
    f.setBold(true);
    titulo_->setFont(f);
    subtitulo_ = new QLabel;
    subtitulo_->setStyleSheet(QStringLiteral("color:#5d6b7a"));
    cifras_ = new QLabel;
    cifras_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    avisos_ = new QLabel;
    avisos_->setWordWrap(true);
    avisos_->setStyleSheet(QStringLiteral("color:#9a4a00; background:#fff3e0; padding:6px;"));
    avisos_->hide();
    raiz->addWidget(titulo_);
    raiz->addWidget(subtitulo_);
    raiz->addWidget(cifras_);
    raiz->addWidget(avisos_);

    auto* barra = new QHBoxLayout;
    buscar_ = new QLineEdit;
    buscar_->setPlaceholderText(QStringLiteral("Buscar por código, descripción, texto o capítulo"));
    buscar_->setClearButtonEnabled(true);
    auto* bArbol = new QPushButton(QStringLiteral("Por capítulos"));
    auto* bLista = new QPushButton(QStringLiteral("Lista"));
    auto* bExp = new QPushButton(QStringLiteral("Expandir"));
    auto* bCol = new QPushButton(QStringLiteral("Colapsar"));
    estado_ = new QLabel;
    barra->addWidget(buscar_, 1);
    barra->addWidget(bArbol);
    barra->addWidget(bLista);
    barra->addWidget(bExp);
    barra->addWidget(bCol);
    barra->addWidget(estado_);
    raiz->addLayout(barra);

    modelo_ = new ModeloPresupuesto(this);
    filtro_ = new FiltroPresupuesto(this);
    filtro_->setSourceModel(modelo_);

    arbol_ = new QTreeView;
    arbol_->setModel(filtro_);
    arbol_->setAlternatingRowColors(true);
    arbol_->setUniformRowHeights(true);
    arbol_->setSelectionBehavior(QAbstractItemView::SelectRows);
    arbol_->setSelectionMode(QAbstractItemView::SingleSelection);
    arbol_->header()->setStretchLastSection(false);
    arbol_->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    arbol_->setColumnWidth(0, 140);

    lista_ = new QStandardItemModel(this);
    tabla_ = new QTableView;
    tabla_->setModel(lista_);
    tabla_->setSortingEnabled(true);
    tabla_->setAlternatingRowColors(true);
    tabla_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tabla_->setSelectionMode(QAbstractItemView::SingleSelection);
    tabla_->horizontalHeader()->setStretchLastSection(false);
    tabla_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    tabla_->setColumnHidden(7, true); // id

    pila_ = new QStackedWidget;
    pila_->addWidget(arbol_);
    pila_->addWidget(tabla_);

    detalle_ = new QWidget;
    auto* detLay = new QVBoxLayout(detalle_);
    detalleTexto_ = new QLabel;
    detalleTexto_->setWordWrap(true);
    detalleTexto_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    tablaDec_ = new QTableWidget(0, 7);
    tablaDec_->setHorizontalHeaderLabels({QStringLiteral("Código"), QStringLiteral("Tipo"), QStringLiteral("Descripción"),
                                           QStringLiteral("Ud"), QStringLiteral("Rendimiento"), QStringLiteral("Precio"),
                                           QStringLiteral("Importe")});
    tablaDec_->horizontalHeader()->setStretchLastSection(true);
    tablaDec_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tablaMed_ = new QTableWidget(0, 5);
    tablaMed_->setHorizontalHeaderLabels({QStringLiteral("Comentario"), QStringLiteral("Uds"), QStringLiteral("Longitud"),
                                           QStringLiteral("Latitud"), QStringLiteral("Altura")});
    tablaMed_->horizontalHeader()->setStretchLastSection(true);
    tablaMed_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    detalleCoste_ = new QLabel;
    detalleCoste_->setStyleSheet(QStringLiteral("color:#5d6b7a"));
    detLay->addWidget(detalleTexto_);
    detLay->addWidget(new QLabel(QStringLiteral("Descompuesto")));
    detLay->addWidget(tablaDec_, 1);
    detLay->addWidget(new QLabel(QStringLiteral("Mediciones")));
    detLay->addWidget(tablaMed_, 1);
    detLay->addWidget(detalleCoste_);

    auto* split = new QSplitter;
    split->addWidget(pila_);
    split->addWidget(detalle_);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 2);
    raiz->addWidget(split, 1);

    gasto_ = new GastoRealWidget;
    pestanas_ = new QTabWidget;
    pestanas_->addTab(central, QStringLiteral("Presupuesto"));
    pestanas_->addTab(gasto_, QStringLiteral("Gasto real"));
    setCentralWidget(pestanas_);

    auto* abrirAcc = new QAction(QStringLiteral("Abrir JSON…"), this);
    abrirAcc->setShortcut(QKeySequence::Open);
    connect(abrirAcc, &QAction::triggered, this, &MainWindow::abrir);
    abrirAcc->setText(QStringLiteral("Abrir presupuesto JSON…"));
    auto* abrirProyAcc = new QAction(QStringLiteral("Abrir proyecto…"), this);
    abrirProyAcc->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+O")));
    connect(abrirProyAcc, &QAction::triggered, this, [this] { abrirProyecto(); });
    auto* guardarAcc = new QAction(QStringLiteral("Guardar proyecto"), this);
    guardarAcc->setShortcut(QKeySequence::Save);
    connect(guardarAcc, &QAction::triggered, this, [this] { guardarProyecto(); });
    auto* guardarComoAcc = new QAction(QStringLiteral("Guardar proyecto como…"), this);
    guardarComoAcc->setShortcut(QKeySequence::SaveAs);
    connect(guardarComoAcc, &QAction::triggered, this, [this] { guardarProyectoComo(); });
    QMenu* menuArchivo = menuBar()->addMenu(QStringLiteral("Archivo"));
    menuArchivo->addAction(abrirAcc);
    menuArchivo->addAction(abrirProyAcc);
    menuArchivo->addSeparator();
    menuArchivo->addAction(guardarAcc);
    menuArchivo->addAction(guardarComoAcc);
    connect(gasto_, &GastoRealWidget::modificado, this, [this] { marcarModificado(true); });
    actualizarTitulo();
    buscar_->setFocus();

    connect(buscar_, &QLineEdit::textChanged, this, &MainWindow::buscar);
    connect(bArbol, &QPushButton::clicked, this, &MainWindow::ponerArbol);
    connect(bLista, &QPushButton::clicked, this, &MainWindow::ponerLista);
    connect(bExp, &QPushButton::clicked, this, &MainWindow::expandir);
    connect(bCol, &QPushButton::clicked, this, &MainWindow::colapsar);
    connect(arbol_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { seleccionCambiada(); });
    connect(tabla_->selectionModel(), &QItemSelectionModel::currentChanged, this, [this] { seleccionCambiada(); });
}

void MainWindow::abrir() {
    const QString ruta = QFileDialog::getOpenFileName(
        this, QStringLiteral("Abrir presupuesto"), {},
        QStringLiteral("JSON de bc3_extraer (*.json);;Todos los ficheros (*)"));
    if (!ruta.isEmpty())
        cargarRuta(ruta);
}

bool MainWindow::cargarPresupuestoDe(const QString& ruta) {
    auto cargado = std::make_unique<Presupuesto>();
    QString error;
    if (!cargarPresupuesto(ruta, cargado.get(), &error)) {
        QMessageBox::warning(this, QStringLiteral("No se pudo abrir"), error);
        return false;
    }
    presupuesto_ = std::move(cargado);
    rutaJson_ = QFileInfo(ruta).absoluteFilePath();
    modelo_->setPresupuesto(presupuesto_.get());
    rellenarLista();
    pintarCabecera();
    gasto_->setReferencia(*presupuesto_);
    arbol_->expandToDepth(0);
    statusBar()->showMessage(ruta);
    return true;
}

void MainWindow::cargarRuta(const QString& ruta) {
    if (!confirmarDescarte())
        return;
    if (ruta.endsWith(QStringLiteral(".bc3proj"), Qt::CaseInsensitive)) {
        cargarProyecto(ruta);
        return;
    }
    if (!cargarPresupuestoDe(ruta))
        return;
    rutaProyecto_.clear();

    // Gastos de la versión anterior (obra.gastos.json junto al presupuesto): se importan
    // y el proyecto queda como "sin guardar" para que los guardes en un .bc3proj.
    const QFileInfo info(ruta);
    QFile antiguo(info.absoluteDir().filePath(info.completeBaseName() + QStringLiteral(".gastos.json")));
    bool importado = false;
    if (antiguo.open(QIODevice::ReadOnly)) {
        const QJsonDocument doc = QJsonDocument::fromJson(antiguo.readAll());
        if (doc.isObject()) {
            gasto_->desdeJson(doc.object());
            importado = true;
        }
    }
    marcarModificado(importado);
    actualizarTitulo();
}

void MainWindow::abrirProyecto() {
    if (!confirmarDescarte())
        return;
    const QString ruta = QFileDialog::getOpenFileName(
        this, QStringLiteral("Abrir proyecto"), {},
        QStringLiteral("Proyecto BC3 (*.bc3proj);;Todos los ficheros (*)"));
    if (!ruta.isEmpty())
        cargarProyecto(ruta);
}

bool MainWindow::cargarProyecto(const QString& ruta) {
    QFile f(ruta);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("No se pudo abrir"), QStringLiteral("No se puede leer %1").arg(ruta));
        return false;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject()) {
        QMessageBox::warning(this, QStringLiteral("No se pudo abrir"),
                             QStringLiteral("%1 no es un proyecto válido.").arg(ruta));
        return false;
    }
    const QJsonObject obj = doc.object();

    // Primero la ruta relativa al proyecto (para poder mover la carpeta entera), luego la absoluta.
    const QDir dir = QFileInfo(ruta).absoluteDir();
    const QString rel = obj.value(QStringLiteral("presupuesto")).toString();
    QString json = rel.isEmpty() ? QString() : dir.absoluteFilePath(rel);
    if (json.isEmpty() || !QFileInfo(json).isFile())
        json = obj.value(QStringLiteral("presupuesto_abs")).toString();
    if (json.isEmpty() || !QFileInfo(json).isFile()) {
        QMessageBox::information(this, QStringLiteral("Proyecto"),
                                 QStringLiteral("No encuentro el presupuesto de este proyecto. Selecciona el JSON."));
        json = QFileDialog::getOpenFileName(
            this, QStringLiteral("Presupuesto del proyecto"), dir.absolutePath(),
            QStringLiteral("JSON de bc3_extraer (*.json);;Todos los ficheros (*)"));
        if (json.isEmpty())
            return false;
    }
    if (!cargarPresupuestoDe(json))
        return false;
    gasto_->desdeJson(obj.value(QStringLiteral("gastos")).toObject());
    rutaProyecto_ = QFileInfo(ruta).absoluteFilePath();
    marcarModificado(false);
    actualizarTitulo();
    statusBar()->showMessage(ruta);
    return true;
}

bool MainWindow::escribirProyecto(const QString& ruta) {
    QJsonObject raiz;
    raiz.insert(QStringLiteral("version"), 1);
    raiz.insert(QStringLiteral("presupuesto"), QFileInfo(ruta).absoluteDir().relativeFilePath(rutaJson_));
    raiz.insert(QStringLiteral("presupuesto_abs"), rutaJson_);
    raiz.insert(QStringLiteral("gastos"), gasto_->aJson());

    QSaveFile f(ruta);
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, QStringLiteral("Guardar proyecto"),
                             QStringLiteral("No se puede escribir %1").arg(ruta));
        return false;
    }
    f.write(QJsonDocument(raiz).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        QMessageBox::warning(this, QStringLiteral("Guardar proyecto"),
                             QStringLiteral("No se pudo guardar %1").arg(ruta));
        return false;
    }
    return true;
}

bool MainWindow::guardarProyecto() {
    if (!presupuesto_) {
        QMessageBox::information(this, QStringLiteral("Guardar proyecto"),
                                 QStringLiteral("Abre primero un presupuesto."));
        return false;
    }
    if (rutaProyecto_.isEmpty())
        return guardarProyectoComo();
    if (!escribirProyecto(rutaProyecto_))
        return false;
    marcarModificado(false);
    statusBar()->showMessage(QStringLiteral("Guardado en %1").arg(rutaProyecto_), 4000);
    return true;
}

bool MainWindow::guardarProyectoComo() {
    if (!presupuesto_) {
        QMessageBox::information(this, QStringLiteral("Guardar proyecto"),
                                 QStringLiteral("Abre primero un presupuesto."));
        return false;
    }
    QString sugerido = rutaProyecto_;
    if (sugerido.isEmpty()) {
        const QFileInfo info(rutaJson_);
        sugerido = info.absoluteDir().filePath(info.completeBaseName() + QStringLiteral(".bc3proj"));
    }
    QString ruta = QFileDialog::getSaveFileName(this, QStringLiteral("Guardar proyecto"), sugerido,
                                                QStringLiteral("Proyecto BC3 (*.bc3proj)"));
    if (ruta.isEmpty())
        return false;
    if (!ruta.endsWith(QStringLiteral(".bc3proj"), Qt::CaseInsensitive))
        ruta += QStringLiteral(".bc3proj");
    if (!escribirProyecto(ruta))
        return false;
    rutaProyecto_ = QFileInfo(ruta).absoluteFilePath();
    marcarModificado(false);
    actualizarTitulo();
    statusBar()->showMessage(QStringLiteral("Guardado en %1").arg(rutaProyecto_), 4000);
    return true;
}

bool MainWindow::confirmarDescarte() {
    if (!modificado_)
        return true;
    QMessageBox caja(QMessageBox::Question, QStringLiteral("Proyecto sin guardar"),
                     QStringLiteral("Hay cambios en el gasto real que no se han guardado."),
                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    caja.setButtonText(QMessageBox::Save, QStringLiteral("Guardar"));
    caja.setButtonText(QMessageBox::Discard, QStringLiteral("Descartar"));
    caja.setButtonText(QMessageBox::Cancel, QStringLiteral("Cancelar"));
    caja.setDefaultButton(QMessageBox::Save);
    switch (caja.exec()) {
    case QMessageBox::Save:
        return guardarProyecto();
    case QMessageBox::Discard:
        return true;
    default:
        return false;
    }
}

void MainWindow::closeEvent(QCloseEvent* evento) {
    if (confirmarDescarte())
        evento->accept();
    else
        evento->ignore();
}

void MainWindow::marcarModificado(bool modificado) {
    modificado_ = modificado;
    setWindowModified(modificado);
}

void MainWindow::actualizarTitulo() {
    QString nombre = QStringLiteral("Visor BC3");
    if (!rutaProyecto_.isEmpty())
        nombre = QFileInfo(rutaProyecto_).completeBaseName();
    else if (presupuesto_ && !presupuesto_->titulo.isEmpty())
        nombre = presupuesto_->titulo;
    setWindowTitle(nombre + QStringLiteral(" [*]"));
}

void MainWindow::pintarCabecera() {
    if (!presupuesto_)
        return;
    titulo_->setText(presupuesto_->titulo.isEmpty() ? QStringLiteral("Presupuesto BC3") : presupuesto_->titulo);
    subtitulo_->setText(unirSubtitulo(presupuesto_->cabecera));
    QStringList cifras;
    cifras << QStringLiteral("Total  %1").arg(formatoEuro(presupuesto_->total));
    cifras << QStringLiteral("Partidas  %1").arg(presupuesto_->nPartidas);
    cifras << QStringLiteral("Capítulos  %1").arg(presupuesto_->nCapitulos);
    if (presupuesto_->coste) {
        const double margen = presupuesto_->total - *presupuesto_->coste;
        cifras << QStringLiteral("Coste  %1").arg(formatoEuro(*presupuesto_->coste));
        const QString pct = presupuesto_->total
            ? QStringLiteral(" (%1 %)").arg(formatoNumero(margen / presupuesto_->total * 100.0, 2))
            : QString();
        cifras << QStringLiteral("Margen  %1%2").arg(formatoEuro(margen), pct);
    }
    cifras_->setText(cifras.join(QStringLiteral("      ")));
    if (presupuesto_->avisos.isEmpty()) {
        avisos_->hide();
    } else {
        avisos_->setText(presupuesto_->avisos.join(QStringLiteral("\n")));
        avisos_->show();
    }
    if (!presupuesto_->cabecera.comentario.trimmed().isEmpty())
        subtitulo_->setToolTip(presupuesto_->cabecera.comentario.trimmed());
    estado_->setText(QStringLiteral("%1 partidas, suma %2")
                         .arg(presupuesto_->nPartidas)
                         .arg(formatoEuro(presupuesto_->total)));
}

void MainWindow::rellenarLista() {
    lista_->clear();
    lista_->setHorizontalHeaderLabels({QStringLiteral("Código"), QStringLiteral("Resumen"), QStringLiteral("Ud"),
                                        QStringLiteral("Cantidad"), QStringLiteral("Precio"), QStringLiteral("Importe"),
                                        QStringLiteral("% del total"), QStringLiteral("id")});
    if (!presupuesto_)
        return;
    for (const Fila& f : presupuesto_->filas) {
        if (f.capitulo)
            continue;
        const double pct = presupuesto_->total > 0 ? f.importe / presupuesto_->total * 100.0 : 0;
        QList<QStandardItem*> row;
        auto texto = [&](const QString& s) {
            auto* it = new QStandardItem(s);
            it->setEditable(false);
            return it;
        };
        auto* cod = texto(f.codigo);
        cod->setData(f.id, Qt::UserRole);
        row << cod;
        row << texto(f.desajuste ? f.resumen + QStringLiteral("  ⚠") : f.resumen);
        row << texto(f.unidad);
        row << texto(formatoNumero(f.cantidad));
        row << texto(f.precio ? formatoNumero(*f.precio, 2) : QString());
        row << texto(formatoEuro(f.importe));
        row << texto(pct > 0 ? formatoNumero(pct, 2) + QStringLiteral(" %") : QString());
        row << texto(QString::number(f.id));
        lista_->appendRow(row);
    }
    tabla_->setColumnHidden(7, true);
}

void MainWindow::buscar(const QString& texto) {
    filtro_->setFilterRegularExpression(QRegularExpression(QRegularExpression::escape(texto.trimmed()),
                                                            QRegularExpression::CaseInsensitiveOption));
    // La lista plana filtra por el texto de las celdas, no por el texto largo de la partida.
    // Para igualar al HTML, se vuelve a marcar las filas cuyo id casa.
    if (!presupuesto_)
        return;
    const QString t = texto.trimmed().toLower();
    const QStringList palabras = t.split(' ', Qt::SkipEmptyParts);
    int visibles = 0;
    double suma = 0;
    for (int r = 0; r < lista_->rowCount(); ++r) {
        const int id = lista_->item(r, 0)->data(Qt::UserRole).toInt();
        const Fila& f = presupuesto_->filas.at(id);
        const bool ok = std::all_of(palabras.cbegin(), palabras.cend(), [&](const QString& w) {
            return f.busqueda.contains(w.normalized(QString::NormalizationForm_D).remove(QRegularExpression("\\p{M}")));
        });
        tabla_->setRowHidden(r, !ok);
        if (ok) {
            ++visibles;
            suma += f.importe;
        }
    }
    if (pila_->currentWidget() == arbol_) {
        int n = 0;
        double s = 0;
        for (const Fila& f : presupuesto_->filas) {
            if (f.capitulo)
                continue;
            const bool ok = palabras.isEmpty() || std::all_of(palabras.cbegin(), palabras.cend(), [&](const QString& w) {
                return f.busqueda.contains(w.toLower().normalized(QString::NormalizationForm_D).remove(QRegularExpression("\\p{M}")));
            });
            if (ok) {
                ++n;
                s += f.importe;
            }
        }
        visibles = n;
        suma = s;
        if (!texto.trimmed().isEmpty())
            arbol_->expandAll();
    }
    estado_->setText(QStringLiteral("%1 de %2 partidas, suma %3")
                         .arg(visibles)
                         .arg(presupuesto_->nPartidas)
                         .arg(formatoEuro(suma)));
}

void MainWindow::seleccionCambiada() {
    if (!presupuesto_)
        return;
    int id = -1;
    if (pila_->currentWidget() == arbol_) {
        const QModelIndex proxy = arbol_->currentIndex();
        const QModelIndex fuente = filtro_->mapToSource(proxy);
        if (const Fila* f = modelo_->filaEn(fuente))
            id = f->id;
    } else if (const QModelIndex idx = tabla_->currentIndex(); idx.isValid()) {
        id = lista_->item(idx.row(), 0)->data(Qt::UserRole).toInt();
    }
    if (id < 0 || id >= presupuesto_->filas.size())
        return;
    pintarDetalle(&presupuesto_->filas.at(id));
}

void MainWindow::pintarDetalle(const Fila* fila) {
    if (!fila || fila->capitulo) {
        detalleTexto_->setText(fila ? fila->resumen : QString());
        tablaDec_->setRowCount(0);
        tablaMed_->setRowCount(0);
        detalleCoste_->clear();
        return;
    }
    QString cab = fila->ruta.isEmpty() ? QString() : fila->ruta.join(QStringLiteral(" › ")) + QStringLiteral("\n");
    detalleTexto_->setText(cab + (fila->texto.isEmpty() ? fila->resumen : fila->texto));

    tablaDec_->setRowCount(fila->descompuestos.size());
    for (int i = 0; i < fila->descompuestos.size(); ++i) {
        const auto& d = fila->descompuestos.at(i);
        tablaDec_->setItem(i, 0, new QTableWidgetItem(QString(d.nivel * 2, ' ') + d.codigo));
        tablaDec_->setItem(i, 1, new QTableWidgetItem(d.tipo));
        tablaDec_->setItem(i, 2, new QTableWidgetItem(d.resumen));
        tablaDec_->setItem(i, 3, new QTableWidgetItem(d.unidad));
        tablaDec_->setItem(i, 4, new QTableWidgetItem(formatoNumero(d.cantidad)));
        tablaDec_->setItem(i, 5, new QTableWidgetItem(d.precio ? formatoNumero(*d.precio, 2) : QString()));
        tablaDec_->setItem(i, 6, new QTableWidgetItem(d.importe ? formatoEuro(*d.importe) : QString()));
    }
    tablaDec_->resizeColumnsToContents();

    int nLin = 0;
    for (const auto& b : fila->mediciones)
        nLin += std::max(1, static_cast<int>(b.lineas.size()));
    tablaMed_->setRowCount(nLin);
    int r = 0;
    for (const auto& b : fila->mediciones) {
        if (b.lineas.isEmpty()) {
            tablaMed_->setItem(r, 0, new QTableWidgetItem(
                (b.posicion.isEmpty() ? QString() : QStringLiteral("Posición ") + b.posicion + QStringLiteral(", "))
                + QStringLiteral("total ") + (b.total ? formatoNumero(*b.total) : QString())));
            ++r;
            continue;
        }
        for (const auto& l : b.lineas) {
            tablaMed_->setItem(r, 0, new QTableWidgetItem(l.comentario));
            tablaMed_->setItem(r, 1, new QTableWidgetItem(l.uds));
            tablaMed_->setItem(r, 2, new QTableWidgetItem(l.longitud));
            tablaMed_->setItem(r, 3, new QTableWidgetItem(l.latitud));
            tablaMed_->setItem(r, 4, new QTableWidgetItem(l.altura));
            ++r;
        }
    }
    if (fila->precioCoste)
        detalleCoste_->setText(QStringLiteral("Precio de coste %1, importe de coste %2")
                                   .arg(formatoNumero(*fila->precioCoste, 2), formatoEuro(fila->importeCoste.value_or(0))));
    else
        detalleCoste_->clear();
    if (fila->texto.isEmpty() && fila->descompuestos.isEmpty() && fila->mediciones.isEmpty() && !fila->precioCoste)
        detalleTexto_->setText(QStringLiteral("Esta partida no trae descompuesto, texto ni mediciones."));
}

void MainWindow::ponerArbol() { pila_->setCurrentWidget(arbol_); }
void MainWindow::ponerLista() { pila_->setCurrentWidget(tabla_); }
void MainWindow::expandir() { arbol_->expandAll(); pila_->setCurrentWidget(arbol_); }
void MainWindow::colapsar() { arbol_->collapseAll(); pila_->setCurrentWidget(arbol_); }
