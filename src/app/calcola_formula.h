/****************************************************************************
** Copyright (c) 2020, Fougue Ltd. <http://www.fougue.pro>
** All rights reserved.
** See license at https://github.com/fougue/mayo/blob/master/LICENSE.txt
****************************************************************************/

#pragma once

#include <QtWidgets/QDialog>

namespace Mayo {

class calcola_formula : public QDialog {
    Q_OBJECT
public:
    calcola_formula(QWidget *parent = nullptr);
    ~calcola_formula();

private:
    //class Ui_calcola_formula* m_ui = nullptr;
};

} // namespace Mayo
