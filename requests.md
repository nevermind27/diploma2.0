
Получение всех тайлов определенного снимка 
GET
/tiles?image_id=<i>
SELECT tile_url FROM Tiles WHERE image_id = <n>


Получение тайлов снимка в порядке по частоте обращения
 GET
/tiles?image_id=123&sort=frequency
SELECT tile_url FROM Tiles WHERE image_id = <id> AND frequency = <frequency>


Добавить тайл в БД
POST
/tiles
 {
  "image_id": <id>,
  "tile_row": <n>,
  "tile_column": <n>
}



"INSERT INTO Tiles (tile_row, tile_column, spectrum, image_id, tile_url) VALUES "


Обновить частоту обращений к тайлу
POST 
/tiles/tile_row/tile_col/increment


UPDATE Tiles SET frequency = <freq>WHERE tile_row = <row> AND tile_col = <col>

