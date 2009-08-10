<?php
// $Id$

/**
 * Main form for query page.
 */
function google_analytics_api_query_form($form_state) {
  $form = array();

  $form['query']['info'] = array(
    '#value' => t('Please see ' . l('Google Analytics Data API Reference', 'http://code.google.com/apis/analytics/docs/gdata/gdataReferenceDimensionsMetrics.html') . ' for a list of dimensions and metrics.'),
  );
  $form['query']['dimensions'] = array(
    '#type' => 'textfield',
    '#title' => t('Dimensions'),
    '#maxlength' => 300,
    '#description' => t('(browser, browserVersion, city, connectionSpeed, continent, date, hostname, pagePath, pageTitle...)'),
  );
  $form['query']['metrics'] = array(
    '#type' => 'textfield',
    '#title' => t('Metrics'),
    '#maxlength' => 300,
    '#description' => t('(entrances, pageviews, timeOnPage, visitors, visits...)'),
  );
  $form['query']['sort_metric'] = array(
    '#type' => 'textfield',
    '#title' => t('Sort Metric'),
    '#maxlength' => 300,
    '#description' => t('(e.g. -entrances, pageviews)'),
  );
  $form['query']['filter'] = array(
    '#type' => 'textfield',
    '#title' => t('Filter'),
    '#maxlength' => 300,
    '#description' => t('(e.g. country == United States && browser == Firefox || browser == Chrome)'),
  );
  $form['query']['start_date'] = array(
    '#type' => 'date',
    '#title' => t('Start Date'),
  );
  $form['query']['no_start_date'] = array(
    '#type' => 'checkbox',
    '#title' => t('No start date.'),
    '#default_value' => 1,
    '#description' => t('(Default: Google Analytics launch date of 1 Jan 2005)'),
  );
  $form['query']['end_date'] = array(
    '#type' => 'date',
    '#title' => t('End Date'),
  );
  $form['query']['no_end_date'] = array(
    '#type' => 'checkbox',
    '#title' => t('No end date.'),
    '#default_value' => 1,
    '#description' => t('(Default: Today)'),
  );
  $form['query']['start_index'] = array(
    '#type' => 'textfield',
    '#title' => t('Start Index'),
    '#default_value' => 1,
  );
  $form['query']['max_results'] = array(
    '#type' => 'textfield',
    '#title' => t('Maximum Results'),
    '#default_value' => 10000,
  );
  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Submit query'),
  );
  $form['#redirect'] = FALSE;

  if (!empty($form_state['post'])) {
    $form['results'] = array(
      '#type' => 'item',
      '#title' => t('Query results'),
      '#value' => _google_analytics_api_query_form_fetch_results($form_state),
    );
  }

  return $form;
}

function _google_analytics_api_query_form_fetch_results($form_state = array()) {
  $dimensions = _google_analytics_api_query_form_process($form_state['post']['dimensions']);
  $metrics = _google_analytics_api_query_form_process($form_state['post']['metrics']);
  $sort_metric = _google_analytics_api_query_form_process($form_state['post']['sort_metric']);
  $filter = $form_state['post']['filter'];
  $start_date = null;
  if ($form_state['post']['no_start_date'] != '1') {
    $start_date =
      $form_state['post']['start_date']['year'] . '-' .
      str_pad($form_state['post']['start_date']['month'], 2, '0', STR_PAD_LEFT) . '-' .
      str_pad($form_state['post']['start_date']['day'], 2, '0', STR_PAD_LEFT);
  }
  $end_date = null;
  if ($form_state['post']['no_end_date'] != '1') {
    $end_date =
      $form_state['post']['end_date']['year'] . '-' .
      str_pad($form_state['post']['end_date']['month'], 2, '0', STR_PAD_LEFT) . '-' .
      str_pad($form_state['post']['end_date']['day'], 2, '0', STR_PAD_LEFT);
  }
  $start_index = $form_state['post']['start_index'];
  $max_results = $form_state['post']['max_results'];
  try {
    $entries = google_analytics_api_report_data(
      $dimensions, $metrics, $sort_metric, $filter,
      $start_date, $end_date, $start_index, $max_results);
  } catch (Exception $e) {
    return $e->getMessage();
  }

  $header = array_merge($dimensions, $metrics);

  $rows = array();
  foreach ($entries as $entry) {
    $row = array();
    $row = array_merge($row, $entry->getDimensions());
    $row = array_merge($row, $entry->getMetrics());
    $rows[] = $row;
  }
  $totals = array(t('Aggregate Value'));
  $totals = array_pad($totals, sizeof($dimensions), '');
  $totals = array_merge($totals, google_analytics_api_gapi()->getMetrics());
  $rows[] = $totals;

  return theme('table', $header, $rows);
}

/**
 * Process a query string.
 */
function _google_analytics_api_query_form_process($string) {
  $array = explode(',', $string);
  $return = array();
  foreach ($array as $value)
    if ($value = trim($value))
      $return[] = $value;
  return $return;
}

/*
 * Return a list of paths that are aliased with the given path (including the given path).
 */
function _google_analytics_api_path_aliases($node_path=null) {
  // Default path is the current path.
  if ($node_path == null)
    $node_path = $_GET['q'];

  // Get the normal node path if it is a node.
  $node_path = drupal_get_normal_path($node_path);

  // Grab all aliases.
  $aliases = array($node_path);
  $result = db_query("SELECT * FROM {url_alias} WHERE src = '%s'", $node_path);
  while ($row = db_fetch_array($result))
    $aliases[] = $row['dst'];

  // If this is the front page, add the base path too, and index.php for good measure.
  // There may be other ways that the user is accessing the front page but we can't
  // account for them all.
  if ($node_path == drupal_get_normal_path(variable_get('site_frontpage', 'node'))) {
    $aliases[] = '';
    $aliases[] = 'index.php';
  }

  return $aliases;
}

/*
 * Construct a filter string that grabs pagePaths corresponding to the specified node or path.
 */
function _google_analytics_api_path_filter($node_path=null) {
  $aliases = _google_analytics_api_path_aliases($node_path);

  // Okay, this is ridiculous.  Google Analytics regex filters
  // have a limit of 32 characters.  Therefore we can't simply
  // create one filter per pagePath.  Instead we are going too
  // do a "contains substring" match on the path, and then take
  // as many of the ending characters paired with ([?#].*)?$. If
  // this will all fit in one regex under 32 characters then so
  // be it.  Return a string if they all do, or an array if not.

  // 32 - 12 = 20 characters maximum for alias
  $under_filters = array();
  $over_filters = array();
  foreach ($aliases as $alias) {
    if (strlen($alias) <= 20) {
      $under_filters[] = 'pagePath =~ ^/' . $alias . '([?#].*)?$';
    } else {
      $over_filters[] = array(
        'pagePath =@ /' . $alias,
        // 32 - 10 = 22 characters
        'pagePath =~ ' . substr($alias, -22) . '([?#].*)?$');
    }
  }

  // Add pipes and put together under and over filters.
  $filters = array();
  if ($under_filters)
    $filters[] = implode(' || ', $under_filters);
  foreach ($over_filters as $filter)
    $filters[] = implode(' && ', $filter);
  return $filters;
}

/*
 * Block for showing statistics of the current path.
 */
function google_analytics_api_block($op='list', $delta=0) {
  switch ($op) {
    case 'list':
      $blocks[0]['info'] = t('Google Analytics Statistics');
      return $blocks;
    case 'view':
      $blocks['subject'] = t('Google Analytics Statistics');
      $blocks['content'] = google_analytics_api_current_summary();
      return $blocks;
  }
}

/*
 * Create a statistics summary block and return it.
 */
function google_analytics_api_current_summary($path=null) {

  // Ensure user has permission to view this block.
  if (!user_access('access Google Analytics data'))
    return false;

  // Ensure the user is logged in.
  if (!google_analytics_api_gapi())
    return 'Please ' . l('grant access', 'admin/settings/google_analytics_api') . ' to your Google account.';

  $data = array();
  foreach (_google_analytics_api_path_filter($path) as $filter) {
    $new_data = google_analytics_api_report_data(
      array('pagePath', 'date'),
      array('pageviews', 'timeOnPage', 'bounces', 'uniquePageviews'),
      array('date', 'pagePath'),
      $filter, strtotime('-7 days'), time(), 1, 1000);
    if ($new_data)
      $data = array_merge($data, $new_data);
  }

  // Check for data.
  if (!$data)
    return '<p>No analytics data is currently available for this path.</p>';

  // Add up all the statistics for different paths.
  $rows = array();
  foreach ($data as $item) {
    $date = strtotime($item->getDate());
    if (isset($rows[$date])) {
      foreach ($item->getMetrics() as $key => $value)
        $rows[$date][$key] += $value;
    } else {
      $rows[$date] = $item->getMetrics();
    }
  }

  // Format and perform calculations to display a chart and table.
  $table_headers = array('Date', 'Page Views', 'Avg. Time', 'Bounce Rate');
  $table_rows = array();
  $chart_page_views = array();;
  foreach ($rows as $date => $row) {
    $text_date = date('m/d', $date);
    $pageviews = $row['pageviews'];
    $avg_time = number_format($row['timeOnPage'] / $row['pageviews'], 1) . 's';
    $bounce_rate = number_format($row['bounces'] / $row['uniquePageviews'] * 100, 2) . '%';
    $table_rows[] = array($text_date, $pageviews, $avg_time, $bounce_rate);

    // Chart stuff.
    $chart_pageviews[] = $pageviews;
    $chart_dates[] = date('M d', $date);
  }
  $table = theme('table', $table_headers, $table_rows);

  // Create a chart.
  $chart = array(
    '#chart_id' => 'daily_page_views',
    '#title' => t('Daily Page Views'),
    '#type' => CHART_TYPE_LINE,
    '#size' => chart_size(350, 200),
    '#adjust_resolution' => TRUE,
  );
  $chart['#data'] = $chart_pageviews;
  $chart['#mixed_axis_labels'][CHART_AXIS_Y_LEFT][0][] = chart_mixed_axis_range_label(0, max($chart_pageviews));
  /* $chart['#mixed_axis_labels'][CHART_AXIS_Y_LEFT][1][] = chart_mixed_axis_label(t('Count'), 95); */
  foreach ($chart_dates as $chart_date)
    $chart['#mixed_axis_labels'][CHART_AXIS_X_BOTTOM][1][] = chart_mixed_axis_label($chart_date);
  /*$chart['#mixed_axis_labels'][CHART_AXIS_X_BOTTOM][2][] = chart_mixed_axis_label(t('Date'), 50);*/

  // See more link.
  $more_link = l(t('See more statistics...'), 'statistics', array('query' => 'path=' . $_GET['q']));

  return chart_render($chart) . $table . $more_link;
}

/*
 * Main statistics page.
 */
function google_analytics_api_statistics_page() {
  $intro = t('<p>More detailed statistics shall go here.  Meanwhile here is what you just saw a second ago.  These paths were searched:</p>');
  $aliases = _google_analytics_api_path_aliases($_GET['path']);
  $aliases_list = theme('item_list', $aliases);
  return $intro . $aliases_list . google_analytics_api_current_summary($_GET['path']);
}