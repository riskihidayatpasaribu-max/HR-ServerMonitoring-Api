using Microsoft.EntityFrameworkCore;
using HR.ServerMonitoring.Api.Data; //menggunakan AppDbContext
using HR.ServerMonitoring.Api.Hubs; //menggunakan SensorHub
using HR.ServerMonitoring.Api.Services;
using HR.ServerMonitoring.Api;

var builder = WebApplication.CreateBuilder(args);

builder.Logging.AddFilter(
    "Microsoft.AspNetCore.Components.Server.Circuits",
    LogLevel.Trace);

builder.Logging.AddFilter(
    "Microsoft.AspNetCore.SignalR",
    LogLevel.Debug);

// Add services to the container.
// Learn more about configuring Swagger/OpenAPI at https://aka.ms/aspnetcore/swashbuckle
builder.Services.AddControllers();
builder.Services.AddHttpClient();
builder.Services.AddDbContext<AppDbContext>(options =>
    options.UseSqlite("Data Source=sensordata.db"));
builder.Services.AddSignalR(); //menambahkan SignalR ke dalam layanan
builder.Services.AddSingleton<DeviceStatusService>();
builder.Services.AddSingleton<MqttBackgroundServices>();

builder.Services.AddSingleton<IMqttCommandService>(
    sp => sp.GetRequiredService<MqttBackgroundServices>());

builder.Services.AddHostedService(
    sp => sp.GetRequiredService<MqttBackgroundServices>());
    
builder.Services.AddRazorComponents()
    .AddInteractiveServerComponents(options =>
    {
        options.DetailedErrors = true;
    });
builder.Services.AddEndpointsApiExplorer();
builder.Services.AddSwaggerGen();

var app = builder.Build();



// Configure the HTTP request pipeline.
if (app.Environment.IsDevelopment())
{
    app.UseSwagger();
    app.UseSwaggerUI();
}

// app.UseHttpsRedirection();
app.UseStaticFiles();
app.UseAntiforgery();
app.MapControllers();

app.MapHub<SensorHub>("/sensorhub"); //menambahkan endpoint untuk hub SignalR
app.MapRazorComponents<App>()
    .AddInteractiveServerRenderMode();

var summaries = new[]
{
    "Freezing", "Bracing", "Chilly", "Cool", "Mild", "Warm", "Balmy", "Hot", "Sweltering", "Scorching"
};

app.MapGet("/weatherforecast", () =>
{
    var forecast =  Enumerable.Range(1, 5).Select(index =>
        new WeatherForecast
        (
            DateOnly.FromDateTime(DateTime.Now.AddDays(index)),
            Random.Shared.Next(-20, 55),
            summaries[Random.Shared.Next(summaries.Length)]
        ))
        .ToArray();
    return forecast;
})
.WithName("GetWeatherForecast")
.WithOpenApi();

app.Run();

record WeatherForecast(DateOnly Date, int TemperatureC, string? Summary)
{
    public int TemperatureF => 32 + (int)(TemperatureC / 0.5556);
}
