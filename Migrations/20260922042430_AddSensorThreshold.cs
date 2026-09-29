using Microsoft.EntityFrameworkCore.Migrations;

#nullable disable

namespace HR.ServerMonitoring.Api.Migrations
{
    /// <inheritdoc />
    public partial class AddSensorThreshold : Migration
    {
        /// <inheritdoc />
        protected override void Up(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.CreateTable(
                name: "SensorThresholds",
                columns: table => new
                {
                    Id = table.Column<int>(type: "INTEGER", nullable: false)
                        .Annotation("Sqlite:Autoincrement", true),
                    TemperatureMin = table.Column<double>(type: "REAL", nullable: false),
                    TemperatureMax = table.Column<double>(type: "REAL", nullable: false),
                    HumidityMin = table.Column<double>(type: "REAL", nullable: false),
                    HumidityMax = table.Column<double>(type: "REAL", nullable: false),
                    GasLevelMin = table.Column<double>(type: "REAL", nullable: false),
                    GasLevelMax = table.Column<double>(type: "REAL", nullable: false)
                },
                constraints: table =>
                {
                    table.PrimaryKey("PK_SensorThresholds", x => x.Id);
                });
        }

        /// <inheritdoc />
        protected override void Down(MigrationBuilder migrationBuilder)
        {
            migrationBuilder.DropTable(
                name: "SensorThresholds");
        }
    }
}
