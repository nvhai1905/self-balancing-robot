#include "MyForm.h"
#include "FormButtonControl.h"
#include "FormGestureControl.h"

using namespace System;
using namespace System::Windows::Forms;

[STAThreadAttribute]
int main(array<String^>^ args)
{
    Application::EnableVisualStyles();
    Application::SetCompatibleTextRenderingDefault(false);

  
    bbb::MyForm form;
    Application::Run(% form);

    return 0;
}
