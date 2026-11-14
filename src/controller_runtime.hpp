#pragma once
namespace sketch {class App;namespace controller {
void poll(App&app,bool foreground);
void synchronize(App&app,bool foreground);
void cancel(App&app);
}}
