# Adding Pages

The display uses ESPHome display pages. Add a new page in
`packages/display_pages.yaml` under `display -> pages`, then switch to it from
the button or from Home Assistant.

Minimal page:

```yaml
- id: page_custom
  lambda: |-
    it.fill(id(paper));
    it.print(20, 20, id(font_title), id(ink), "Custom");
```

To show a Home Assistant entity:

1. Add it in `packages/ha_entities.yaml`.
2. Give it an ESPHome `id`.
3. Print it in the page lambda.

Example:

```yaml
sensor:
  - platform: homeassistant
    id: office_co2
    entity_id: sensor.office_co2
    internal: true
```

```cpp
it.printf(20, 100, id(font_body), id(ink), "CO2: %.0f ppm", id(office_co2).state);
```

